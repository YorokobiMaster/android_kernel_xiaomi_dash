// SPDX-License-Identifier: GPL-2.0
/* Present orange to first-stage PID 1 through /proc/bootconfig. */
#include <linux/android_dsu_avb.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <trace/events/sched.h>

#define SYSTEM_INIT_PATH "/system/bin/init"

static atomic_t first_stage_active = ATOMIC_INIT(1);

bool android_dsu_avb_bootconfig_active(void)
{
	return current->pid == 1 && atomic_read(&first_stage_active);
}

static void dsu_avb_process_exec(void *unused, struct task_struct *task,
				 pid_t old_pid, struct linux_binprm *bprm)
{
	(void)unused;
	(void)old_pid;

	if (task->pid != 1 || !bprm || !bprm->filename ||
	    strcmp(bprm->filename, SYSTEM_INIT_PATH))
		return;

	if (atomic_cmpxchg(&first_stage_active, 1, 0) == 1)
		pr_info("android-dsu-avb: first-stage orange view closed\n");
}

static int __init android_dsu_avb_init(void)
{
	int error;

	error = register_trace_sched_process_exec(dsu_avb_process_exec, NULL);
	if (error) {
		atomic_set(&first_stage_active, 0);
		pr_err("android-dsu-avb: failed to register init exec gate: %d\n",
		       error);
		return error;
	}

	pr_info("android-dsu-avb: first-stage orange view enabled\n");
	return 0;
}
subsys_initcall(android_dsu_avb_init);
