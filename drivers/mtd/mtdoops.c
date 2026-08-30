// SPDX-License-Identifier: GPL-2.0-only
/*
 * MTD Oops/Panic logger
 *
 * Copyright © 2007 Nokia Corporation. All rights reserved.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timekeeping.h>
#include <linux/mtd/mtd.h>
#include <linux/kmsg_dump.h>

/* Maximum MTD partition size */
#define MTDOOPS_MAX_MTD_SIZE (16 * 1024 * 1024)
#define MTDOOPS_KMSG_SIZE (512 * 1024)
#define MTDOOPS_PMSG_SIG 0x43474244

static unsigned long record_size = 4096;
module_param(record_size, ulong, 0400);
MODULE_PARM_DESC(record_size,
		"record size for MTD OOPS pages in bytes (default 4096)");

static char mtddev[80];
module_param_string(mtddev, mtddev, 80, 0400);
MODULE_PARM_DESC(mtddev,
		"name or index number of the MTD device to use");

static int dump_oops = 1;
module_param(dump_oops, int, 0600);
MODULE_PARM_DESC(dump_oops,
		"set to 1 to dump oopses, 0 to only dump panics (default 1)");

static char build_fingerprint[256];
module_param_string(fingerprint, build_fingerprint,
		    sizeof(build_fingerprint), 0644);

static int boot_mode;
module_param(boot_mode, int, 0600);
MODULE_PARM_DESC(boot_mode, "boot_mode (default 0)");

#define MTDOOPS_KERNMSG_MAGIC_v1 0x5d005d00  /* Original */
#define MTDOOPS_KERNMSG_MAGIC_v2 0x5d005e00  /* Adds the timestamp */

struct mtdoops_hdr {
	u32 seq;
	u32 magic;
	ktime_t timestamp;
} __packed;

struct mtdoops_pmsg_data {
	/* Retained by the stock layout before the active DT fields. */
	u8 reserved[24];
	unsigned long mem_size;
	phys_addr_t mem_address;
	unsigned long console_size;
	unsigned long pmsg_size;
};

struct mtdoops_pmsg_hdr {
	u32 signature;
	s32 start;
	s32 size;
	u8 data[];
};

static struct mtdoops_context {
	struct kmsg_dumper dump;
	struct notifier_block reboot_nb;
	struct mtdoops_pmsg_data pmsg_data;

	int mtd_index;
	struct work_struct work_erase;
	struct work_struct work_write;
	struct mtd_info *mtd;
	int oops_pages;
	int nextpage;
	int nextcount;
	unsigned long *oops_page_used;

	unsigned long oops_buf_busy;
	void *oops_buf;
} oops_cxt;

static void mark_page_used(struct mtdoops_context *cxt, int page)
{
	set_bit(page, cxt->oops_page_used);
}

static void mark_page_unused(struct mtdoops_context *cxt, int page)
{
	clear_bit(page, cxt->oops_page_used);
}

static int page_is_used(struct mtdoops_context *cxt, int page)
{
	return test_bit(page, cxt->oops_page_used);
}

static int mtdoops_erase_block(struct mtdoops_context *cxt, int offset)
{
	struct mtd_info *mtd = cxt->mtd;
	u32 start_page_offset = mtd_div_by_eb(offset, mtd) * mtd->erasesize;
	u32 start_page = start_page_offset / record_size;
	u32 erase_pages = mtd->erasesize / record_size;
	struct erase_info erase;
	int ret;
	int page;

	erase.addr = offset;
	erase.len = mtd->erasesize;

	ret = mtd_erase(mtd, &erase);
	if (ret) {
		pr_warn("erase of region [0x%llx, 0x%llx] on \"%s\" failed\n",
			(unsigned long long)erase.addr,
			(unsigned long long)erase.len, mtddev);
		return ret;
	}

	/* Mark pages as unused */
	for (page = start_page; page < start_page + erase_pages; page++)
		mark_page_unused(cxt, page);

	return 0;
}

static void mtdoops_erase(struct mtdoops_context *cxt)
{
	struct mtd_info *mtd = cxt->mtd;
	int i = 0, j, ret, mod;

	/* We were unregistered */
	if (!mtd)
		return;

	mod = (cxt->nextpage * record_size) % mtd->erasesize;
	if (mod != 0) {
		cxt->nextpage = cxt->nextpage + ((mtd->erasesize - mod) / record_size);
		if (cxt->nextpage >= cxt->oops_pages)
			cxt->nextpage = 0;
	}

	while ((ret = mtd_block_isbad(mtd, cxt->nextpage * record_size)) > 0) {
badblock:
		pr_warn("bad block at %08lx\n",
			cxt->nextpage * record_size);
		i++;
		cxt->nextpage = cxt->nextpage + (mtd->erasesize / record_size);
		if (cxt->nextpage >= cxt->oops_pages)
			cxt->nextpage = 0;
		if (i == cxt->oops_pages / (mtd->erasesize / record_size)) {
			pr_err("all blocks bad!\n");
			return;
		}
	}

	if (ret < 0) {
		pr_err("mtd_block_isbad failed, aborting\n");
		return;
	}

	for (j = 0, ret = -1; (j < 3) && (ret < 0); j++)
		ret = mtdoops_erase_block(cxt, cxt->nextpage * record_size);

	if (ret >= 0) {
		pr_debug("ready %d, %d\n",
			 cxt->nextpage, cxt->nextcount);
		return;
	}

	if (ret == -EIO) {
		ret = mtd_block_markbad(mtd, cxt->nextpage * record_size);
		if (ret < 0 && ret != -EOPNOTSUPP) {
			pr_err("block_markbad failed, aborting\n");
			return;
		}
	}
	goto badblock;
}

/* Scheduled work - when we can't proceed without erasing a block */
static void mtdoops_workfunc_erase(struct work_struct *work)
{
	struct mtdoops_context *cxt =
			container_of(work, struct mtdoops_context, work_erase);
	mtdoops_erase(cxt);
}

static void mtdoops_inc_counter(struct mtdoops_context *cxt, int panic)
{
	cxt->nextpage++;
	if (cxt->nextpage >= cxt->oops_pages)
		cxt->nextpage = 0;
	cxt->nextcount++;
	if (cxt->nextcount == 0xffffffff)
		cxt->nextcount = 0;

	if (page_is_used(cxt, cxt->nextpage)) {
		pr_debug("not ready %d, %d (erase %s)\n",
			 cxt->nextpage, cxt->nextcount,
			 panic ? "immediately" : "scheduled");
		if (panic) {
			/* In case of panic, erase immediately */
			mtdoops_erase(cxt);
		} else {
			/* Otherwise, schedule work to erase it "nicely" */
			schedule_work(&cxt->work_erase);
		}
	} else {
		pr_debug("ready %d, %d (no erase)\n",
			 cxt->nextpage, cxt->nextcount);
	}
}

static void mtdoops_write(struct mtdoops_context *cxt)
{
	struct mtd_info *mtd = cxt->mtd;
	size_t retlen;
	struct mtdoops_hdr *hdr;
	int ret;

	if (test_and_set_bit(0, &cxt->oops_buf_busy))
		return;

	/* Add mtdoops header to the buffer */
	hdr = (struct mtdoops_hdr *)cxt->oops_buf;
	hdr->seq = cxt->nextcount;
	hdr->magic = MTDOOPS_KERNMSG_MAGIC_v2;
	hdr->timestamp = ktime_get_real();

	ret = mtd_write(mtd, cxt->nextpage * record_size,
			record_size, &retlen, cxt->oops_buf);

	if (retlen != record_size || ret < 0) {
		pr_err("write failure at %ld (%td of %ld written), error %d\n",
		       cxt->nextpage * record_size, retlen, record_size, ret);
	} else {
		/* Ensure block2mtd's dirty pages survive the following reboot. */
		mtd_sync(mtd);
	}
	mark_page_used(cxt, cxt->nextpage);
	clear_bit(0, &cxt->oops_buf_busy);
}

static void mtdoops_workfunc_write(struct work_struct *work)
{
	struct mtdoops_context *cxt =
			container_of(work, struct mtdoops_context, work_write);

	mtdoops_write(cxt);
}

static void find_next_position(struct mtdoops_context *cxt)
{
	struct mtd_info *mtd = cxt->mtd;
	struct mtdoops_hdr hdr;
	int ret, page, maxpos = 0;
	u32 maxcount = 0xffffffff;
	size_t retlen;

	for (page = 0; page < cxt->oops_pages; page++) {
		if (mtd_block_isbad(mtd, page * record_size))
			continue;
		/* Assume the page is used */
		mark_page_used(cxt, page);
		ret = mtd_read(mtd, page * record_size, sizeof(hdr),
			       &retlen, (u_char *)&hdr);
		if (retlen != sizeof(hdr) ||
				(ret < 0 && !mtd_is_bitflip(ret))) {
			pr_err("read failure at %ld (%zu of %zu read), err %d\n",
			       page * record_size, retlen, sizeof(hdr), ret);
			continue;
		}

		if (hdr.seq == 0xffffffff && hdr.magic == 0xffffffff)
			mark_page_unused(cxt, page);
		if (hdr.seq == 0xffffffff ||
		    (hdr.magic != MTDOOPS_KERNMSG_MAGIC_v1 &&
		     hdr.magic != MTDOOPS_KERNMSG_MAGIC_v2))
			continue;
		if (maxcount == 0xffffffff) {
			maxcount = hdr.seq;
			maxpos = page;
		} else if (hdr.seq < 0x40000000 && maxcount > 0xc0000000) {
			maxcount = hdr.seq;
			maxpos = page;
		} else if (hdr.seq > maxcount && hdr.seq < 0xc0000000) {
			maxcount = hdr.seq;
			maxpos = page;
		} else if (hdr.seq > maxcount && hdr.seq > 0xc0000000
					&& maxcount > 0x80000000) {
			maxcount = hdr.seq;
			maxpos = page;
		}
	}
	if (maxcount == 0xffffffff) {
		cxt->nextpage = cxt->oops_pages - 1;
		cxt->nextcount = 0;
	} else {
		cxt->nextpage = maxpos;
		cxt->nextcount = maxcount;
	}

	mtdoops_inc_counter(cxt, 0);
}

static void mtdoops_do_null(struct kmsg_dumper *dumper,
			    enum kmsg_dump_reason reason)
{
}

static void *ram_vmap(phys_addr_t start, size_t size)
{
	unsigned long offset = offset_in_page(start);
	phys_addr_t page_start = start - offset;
	unsigned int page_count = DIV_ROUND_UP(size + offset, PAGE_SIZE);
	struct page **pages;
	void *vaddr;
	unsigned int i;

	pages = kmalloc_array(page_count, sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n",
		       __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++)
		pages[i] = pfn_to_page(PHYS_PFN(page_start) + i);

	vaddr = vmap(pages, page_count, VM_IOREMAP | VM_MAP, PAGE_KERNEL);
	kfree(pages);

	return vaddr + offset;
}

static const char * const kdump_reason[] = {
	"Unknown",
	"Kernel Panic",
	"Oops!",
	"Emerg",
	"Shut Down",
	"Restart",
	"PowerOff",
	"Long Press",
};

#define MTDOOPS_REPORT_START "\n ---mtdoops report start--- \n"
#define MTDOOPS_REPORT_FORMAT \
	"\n```\n" \
	"## Oops_Index: %d\n" \
	"### Build: %s\n" \
	"## REASON: %s\n" \
	"#### LOG TYPE:%s\n" \
	"## BOOT MODE:%s\n" \
	"##### %04ld-%02d-%02d %02d:%02d:%02d\n" \
	"```c\n"
#define MTDOOPS_LOGCAT_FORMAT \
	"\n```\n" \
	"#### LOG TYPE:%s\n" \
	"#####%04ld-%02d-%02d %02d:%02d:%02d\n" \
	"```\n"

static void mtdoops_do_dump(int reason)
{
	static int do_dump_count;
	struct mtdoops_context *cxt = &oops_cxt;
	struct mtdoops_pmsg_data *pmsg_data = &cxt->pmsg_data;
	struct mtdoops_pmsg_hdr *pmsg;
	struct kmsg_dump_iter iter;
	phys_addr_t pstart;
	char boot_reason[17] = { 0 };
	size_t kmsg_len = 0;
	int info_len;
	int copy_len;
	int tail_len;
	int i;

	do_dump_count++;
	pr_err("%s start , count = %d , page = %d, reason = %d, dump_count = %d\n",
	       __func__, cxt->nextcount, cxt->nextpage, reason,
	       do_dump_count);

	if (do_dump_count >= 2) {
		for (i = 0; i < 3; i++) {
			if (mtdoops_erase_block(cxt,
						cxt->nextpage * record_size) >= 0)
				break;
		}
	}

	kmsg_dump_rewind(&iter);
	if (test_and_set_bit(0, &cxt->oops_buf_busy))
		return;
	kmsg_dump_get_buffer(&iter, true, cxt->oops_buf + 8,
			     MTDOOPS_KMSG_SIZE - 8, &kmsg_len);
	clear_bit(0, &cxt->oops_buf_busy);

	{
		struct timespec64 ts;
		struct tm tm;
		char info[512];

		memset(info, 0, sizeof(info));
		ktime_get_coarse_real_ts64(&ts);
		time64_to_tm(ts.tv_sec + 8 * 60 * 60, 0, &tm);

		switch (boot_mode) {
		case 0:
			memcpy(boot_reason, "normal", sizeof("normal"));
			break;
		case 1:
			memcpy(boot_reason, "recovery", sizeof("recovery"));
			break;
		case 2:
			memcpy(boot_reason, "poweroff_charger",
			       sizeof("poweroff_charger"));
			break;
		}

		memcpy(cxt->oops_buf + 8, MTDOOPS_REPORT_START,
		       sizeof(MTDOOPS_REPORT_START) - 1);
		info_len = snprintf(info, 200, MTDOOPS_REPORT_FORMAT,
				    cxt->nextcount, build_fingerprint,
				    kdump_reason[reason], "LAST KMSG",
				    boot_reason, tm.tm_year + 1900,
				    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
				    tm.tm_min, tm.tm_sec);
		info_len = min_t(unsigned int, info_len, sizeof(info));
		memcpy(cxt->oops_buf + 8 + sizeof(MTDOOPS_REPORT_START) - 1,
		       info, info_len);
	}

	pstart = pmsg_data->mem_address + pmsg_data->mem_size -
		 pmsg_data->pmsg_size - PAGE_SIZE;
	pr_err("pstart = 0x%llx \n", pstart);
	pr_err("dstart = 0x%lx \n", pmsg_data->pmsg_size);
	pmsg = ram_vmap(pstart, pmsg_data->pmsg_size);
	pr_err("mtdoops_do_dump pmsg paddr = 0x%p \n", pmsg);

	if (pmsg->signature != MTDOOPS_PMSG_SIG) {
		pr_err("mtdoops: read pmsg failed sig = 0x%x \n",
		       pmsg->signature);
		goto write;
	}

	copy_len = min_t(int, pmsg->size, record_size - kmsg_len - 8);
	if (copy_len > pmsg->start) {
		tail_len = copy_len - pmsg->start;
		memcpy(cxt->oops_buf + kmsg_len + 8,
		       pmsg->data + pmsg->size - tail_len, tail_len);
		memcpy(cxt->oops_buf + kmsg_len + 8 + tail_len,
		       pmsg->data, pmsg->start);
	} else {
		memcpy(cxt->oops_buf + kmsg_len + 8,
		       pmsg->data, copy_len);
	}

	{
		struct timespec64 ts;
		struct tm tm;
		char logcat_info[80] = { 0 };

		ktime_get_coarse_real_ts64(&ts);
		time64_to_tm(ts.tv_sec + 8 * 60 * 60, 0, &tm);
		info_len = snprintf(logcat_info, sizeof(logcat_info),
				    MTDOOPS_LOGCAT_FORMAT, "LAST LOGCAT",
				    tm.tm_year + 1900, tm.tm_mon + 1,
				    tm.tm_mday, tm.tm_hour, tm.tm_min,
				    tm.tm_sec);
		memcpy(cxt->oops_buf + kmsg_len + 8, logcat_info, info_len);
	}

write:
	mtdoops_write(cxt);
	pr_err("mtdoops_do_dump() finish \n");
}

static int mtdoops_reboot_nb_handle(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	int reason;

	if (action == SYS_RESTART)
		reason = 5;
	else if (action == SYS_POWER_OFF)
		reason = 6;
	else
		return NOTIFY_OK;

	if (oops_cxt.mtd)
		mtdoops_do_dump(reason);

	return NOTIFY_OK;
}

static int mtdoops_parse_dt_u32(struct platform_device *pdev,
				const char *propname, u32 default_value,
				u32 *value)
{
	u32 val32 = 0;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, propname, &val32);
	if (ret == -EINVAL) {
		val32 = default_value;
	} else if (ret < 0) {
		pr_err("failed to parse property %s: %d\n", propname, ret);
		return ret;
	}

	if (val32 > INT_MAX) {
		pr_err("%s %u > INT_MAX\n", propname, val32);
		return -EOVERFLOW;
	}

	*value = val32;
	return 0;
}

static int mtdoops_pmsg_probe(struct platform_device *pdev)
{
	struct mtdoops_pmsg_data *pmsg_data = &oops_cxt.pmsg_data;
	struct resource *res;
	u32 value;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("failed to locate DT /reserved-memory resource\n");
		return -EINVAL;
	}

	pmsg_data->mem_size = resource_size(res);
	pmsg_data->mem_address = res->start;

	ret = mtdoops_parse_dt_u32(pdev, "console-size", 0, &value);
	if (ret)
		return ret;
	pmsg_data->console_size = value;

	ret = mtdoops_parse_dt_u32(pdev, "pmsg-size", 0, &value);
	if (ret)
		return ret;
	pmsg_data->pmsg_size = value;

	pr_err("pares mtd_dt, mem_address =0x%llx, mem_size =0x%lx \n",
	       pmsg_data->mem_address, pmsg_data->mem_size);
	pr_err("pares mtd_dt, pmsg_size =0x%lx, console-size =0x%lx \n",
	       pmsg_data->pmsg_size, pmsg_data->console_size);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "xiaomi,mtdoops_pmsg" },
	{ }
};

static struct platform_driver mtdoops_pmsg_driver = {
	.probe = mtdoops_pmsg_probe,
	.driver = {
		.name = "mtdoops_pmsg",
		.of_match_table = dt_match,
	},
};

static void mtdoops_notify_add(struct mtd_info *mtd)
{
	struct mtdoops_context *cxt = &oops_cxt;
	u64 mtdoops_pages = div_u64(mtd->size, record_size);
	int err;

	if (!strcmp(mtd->name, mtddev))
		cxt->mtd_index = mtd->index;

	if (mtd->index != cxt->mtd_index || cxt->mtd_index < 0)
		return;

	if (mtd->size < mtd->erasesize * 2) {
		pr_err("MTD partition %d not big enough for mtdoops\n",
		       mtd->index);
		return;
	}
	if (mtd->erasesize < record_size) {
		pr_err("eraseblock size of MTD partition %d too small\n",
		       mtd->index);
		return;
	}
	if (mtd->size > MTDOOPS_MAX_MTD_SIZE) {
		pr_err("mtd%d is too large (limit is %d MiB)\n",
		       mtd->index, MTDOOPS_MAX_MTD_SIZE / 1024 / 1024);
		return;
	}

	/* oops_page_used is a bit field */
	cxt->oops_page_used =
		vmalloc(array_size(sizeof(unsigned long),
				   DIV_ROUND_UP(mtdoops_pages,
						BITS_PER_LONG)));
	if (!cxt->oops_page_used) {
		pr_err("could not allocate page array\n");
		return;
	}

	cxt->dump.max_reason = KMSG_DUMP_MAX;
	cxt->dump.dump = mtdoops_do_null;
	err = kmsg_dump_register(&cxt->dump);
	if (err) {
		pr_err("registering kmsg dumper failed, error %d\n", err);
		vfree(cxt->oops_page_used);
		cxt->oops_page_used = NULL;
		return;
	}

	cxt->reboot_nb.notifier_call = mtdoops_reboot_nb_handle;
	cxt->reboot_nb.priority = 255;
	register_reboot_notifier(&cxt->reboot_nb);

	cxt->mtd = mtd;
	cxt->oops_pages = (int)mtd->size / record_size;
	find_next_position(cxt);
	pr_info("Attached to MTD device %d\n", mtd->index);
}

static void mtdoops_notify_remove(struct mtd_info *mtd)
{
	struct mtdoops_context *cxt = &oops_cxt;

	if (mtd->index != cxt->mtd_index || cxt->mtd_index < 0)
		return;

	if (kmsg_dump_unregister(&cxt->dump) < 0)
		pr_warn("could not unregister kmsg_dumper\n");
	unregister_reboot_notifier(&cxt->reboot_nb);

	cxt->mtd = NULL;
	flush_work(&cxt->work_erase);
	flush_work(&cxt->work_write);
}

static struct mtd_notifier mtdoops_notifier = {
	.add	= mtdoops_notify_add,
	.remove	= mtdoops_notify_remove,
};

static int __init mtdoops_init(void)
{
	struct mtdoops_context *cxt = &oops_cxt;
	int mtd_index;
	char *endp;

	if (strlen(mtddev) == 0) {
		pr_err("mtd device (mtddev=name/number) must be supplied\n");
		return -EINVAL;
	}
	if ((record_size & 4095) != 0) {
		pr_err("record_size must be a multiple of 4096\n");
		return -EINVAL;
	}
	if (record_size < 4096) {
		pr_err("record_size must be over 4096 bytes\n");
		return -EINVAL;
	}

	/* Setup the MTD device to use */
	cxt->mtd_index = -1;
	mtd_index = simple_strtoul(mtddev, &endp, 0);
	if (*endp == '\0')
		cxt->mtd_index = mtd_index;

	cxt->oops_buf = kmalloc(record_size, GFP_KERNEL);
	if (!cxt->oops_buf)
		return -ENOMEM;
	memset(cxt->oops_buf, 0xff, record_size);
	cxt->oops_buf_busy = 0;

	INIT_WORK(&cxt->work_erase, mtdoops_workfunc_erase);
	INIT_WORK(&cxt->work_write, mtdoops_workfunc_write);

	platform_driver_register(&mtdoops_pmsg_driver);
	register_mtd_user(&mtdoops_notifier);
	return 0;
}

static void __exit mtdoops_exit(void)
{
	struct mtdoops_context *cxt = &oops_cxt;

	unregister_mtd_user(&mtdoops_notifier);
	kfree(cxt->oops_buf);
	vfree(cxt->oops_page_used);
}

module_init(mtdoops_init);
module_exit(mtdoops_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Purdie <rpurdie@openedhand.com>");
MODULE_DESCRIPTION("MTD Oops/Panic console logger/driver");
