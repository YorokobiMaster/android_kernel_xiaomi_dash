/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ANDROID_DSU_AVB_H
#define _LINUX_ANDROID_DSU_AVB_H

#include <linux/kconfig.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_ANDROID_DSU_AVB_BYPASS)
bool android_dsu_avb_bootconfig_active(void);
#else
static inline bool android_dsu_avb_bootconfig_active(void)
{
	return false;
}
#endif

#endif /* _LINUX_ANDROID_DSU_AVB_H */
