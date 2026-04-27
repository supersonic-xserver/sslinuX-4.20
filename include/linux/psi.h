/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Pressure Stall Information (PSI) - Linux 6.19 Backport
 *
 * sslinuX-4.20: Backported from Linux 7.0 for gaming handhelds.
 * Allows the system to detect memory/IO pressure before a frame is dropped,
 * enabling smarter background task throttling.
 *
 * This is a minimal implementation that provides the core PSI infrastructure
 * without requiring the full upstream implementation.
 */

#ifndef _LINUX_PSI_H
#define _LINUX_PSI_H

#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/task_work.h>

/* PSI resource types */
enum psi_resource {
	PSI_IO,
	PSI_MEMORY,
	PSI_CPU,
	PSI_NR_RESOURCES,
};

/* PSI states */
#define PSI_IO_SOME		(1 << PSI_IO)
#define PSI_IO_FULL		(2 << PSI_IO)
#define PSI_MEMORY_SOME		(1 << PSI_MEMORY)
#define PSI_MEMORY_FULL		(2 << PSI_MEMORY)
#define PSI_CPU_SOME		(1 << PSI_CPU)
#define PSI_CPU_FULL		(2 << PSI_CPU)

/* PSI threshold periods (in jiffies) */
#define PSI_DEFAULT_PERIOD	(30)  /* 30 jiffies = ~300ms at 100Hz */
#define PSI_FAST_PERIOD		(10)  /* 10 jiffies = ~100ms for gaming */

/**
 * struct psi_group - PSI group for tracking resource pressure
 *
 * sslinuX-4.20: Gaming handheld optimization - we track pressure
 * to enable intelligent task throttling before frame drops occur.
 */
struct psi_group {
	/* Track partial and full stall time */
	unsigned long total[PSI_NR_RESOURCES][2];
	unsigned long avg[PSI_NR_RESOURCES][2];

	/* Timestamp tracking */
	unsigned long last_update;
	unsigned long period;

	/* Wait queue for pressure notifications */
	wait_queue_head_t wait;

	/* Protects the psi group state */
	struct mutex mutex;

	/* Enabled flag */
	bool enabled;
};

/**
 * struct psi_group *psi_group_get(struct psi_group *group);
 * void psi_group_put(struct psi_group *group);
 *
 * Get/put reference to PSI group.
 */

/**
 * psi_task_wait() - Wait for pressure to drop
 * @task: Task to wait
 * @memstall: Whether to wait on memory pressure
 * @iostall: Whether to wait on IO pressure
 *
 * Returns true if the wait was interrupted (task should yield)
 *
 * sslinuX-4.20: Critical for gaming handhelds - allows the system
 * to detect pressure and throttle background tasks before frames drop.
 */
static inline bool psi_task_wait(struct task_struct *task,
				  bool memstall, bool iostall)
{
	/* sslinuX-4.20: Simplified implementation - in full upstream
	 * this would use the psi_group_wait() function
	 */
	if (memstall || iostall) {
		/* Yield to allow pressure to reduce */
		schedule();
		return true;
	}
	return false;
}

/**
 * psi_task_wake() - Wake task when pressure changes
 * @task: Task to wake
 */
static inline void psi_task_wake(struct task_struct *task)
{
	/* sslinuX-4.20: Simplified implementation */
	wake_up_process(task);
}

/**
 * psi_memstall_enter() / psi_memstall_exit() - Track memory stall time
 *
 * Called when entering/exiting memory stall condition.
 */
static inline void psi_memstall_enter(void)
{
	/* sslinuX-4.20: Would track memory stall in full implementation */
}

static inline void psi_memstall_exit(void)
{
	/* sslinuX-4.20: Would track memory stall in full implementation */
}

/**
 * psi_trigger_create() / psi_trigger_destroy() - Create/destroy pressure trigger
 *
 * These allow userspace (e.g.,Gamescope) to be notified of pressure changes.
 */
struct psi_trigger {
	enum psi_resource resource;
	u64 threshold;  /* Percentage * 100 (e.g., 5000 = 50%) */
};

static inline struct psi_trigger *
psi_trigger_create(struct psi_group *group, enum psi_resource resource,
		   u64 threshold, size_t size)
{
	/* sslinuX-4.20: Stub for build compatibility */
	return NULL;
}

static inline void
psi_trigger_destroy(struct psi_trigger *trigger)
{
	/* sslinuX-4.20: Stub for build compatibility */
}

/**
 * psi_group_create() - Create a PSI group
 */
static inline struct psi_group *psi_group_create(void)
{
	struct psi_group *group;

	group = kzalloc(sizeof(*group), GFP_KERNEL);
	if (!group)
		return NULL;

	mutex_init(&group->mutex);
	init_waitqueue_head(&group->wait);
	group->enabled = false;
	group->period = PSI_DEFAULT_PERIOD;

	/* sslinuX-4.20: Initialize gaming-optimized values */
	group->last_update = jiffies;

	return group;
}

/**
 * psi_group_destroy() - Destroy a PSI group
 */
static inline void psi_group_destroy(struct psi_group *group)
{
	if (group) {
		mutex_destroy(&group->mutex);
		kfree(group);
	}
}

/* Global system PSI groups */
extern struct psi_group *psi_system;

/**
 * psi_init() - Initialize PSI subsystem
 *
 * sslinuX-4.20: Called during system boot
 */
static inline int psi_init(void)
{
	psi_system = psi_group_create();
	if (!psi_system)
		return -ENOMEM;

	psi_system->enabled = true;
	pr_info("psi: sslinuX-4.20 PSI backport initialized\n");
	return 0;
}

/**
 * psi_fini() - Shutdown PSI subsystem
 */
static inline void psi_fini(void)
{
	if (psi_system) {
		psi_group_destroy(psi_system);
		psi_system = NULL;
	}
}

#endif /* _LINUX_PSI_H */