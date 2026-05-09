/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * ntsync.h - NT synchronization primitive driver UAPI
 *
 * Provides ioctls for emulating NT synchronization primitives
 * (semaphores, mutexes, events, and wait-all) used by Wine/Proton.
 *
 * ioctl numbers are in the misc range (0x00-0x7F) to avoid conflicts.
 */

#ifndef _UAPI_LINUX_NTSYNC_H
#define _UAPI_LINUX_NTSYNC_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* Device node */
#define NTSYNC_DEVICE_NAME	"ntsync"

/* ioctl numbers - misc device range (0x00-0x7F) */
#define NTSYNC_IOC_BASE		'N'

/* Object types */
#define NTSYNC_TYPE_SEM		1
#define NTSYNC_TYPE_MUTEX	2
#define NTSYNC_TYPE_EVENT	3

/* Event types */
#define NTSYNC_EVENT_AUTO	0
#define NTSYNC_EVENT_MANUAL	1

/* Wait flags */
#define NTSYNC_WAIT_ALL		0x01
#define NTSYNC_WAIT_REALTIME	0x02

/**
 * struct ntsync_sem_args - Create/open a semaphore
 * @initial: Initial count value
 * @max: Maximum count value
 */
struct ntsync_sem_args {
	__u32	initial;
	__u32	max;
};

/**
 * struct ntsync_mutex_args - Create/open a mutex
 * @owner: Owning process ID (0 for new mutex)
 */
struct ntsync_mutex_args {
	__u32	owner;
};

/**
 * struct ntsync_event_args - Create/open an event
 * @manual_reset: Event type (auto or manual reset)
 * @signaled: Initial signaled state
 */
struct ntsync_event_args {
	__u32	manual_reset;
	__u32	signaled;
};

/**
 * struct ntsync_wait_args - Wait for objects
 * @timeout: Timeout in nanoseconds (absolute)
 * @count: Number of objects to wait on
 * @handles: Array of object handles
 * @forall: Wait for all objects (NTSYNC_WAIT_ALL)
 * @ signaled: Returns which objects are signaled
 */
struct ntsync_wait_args {
	__u64	timeout;
	__u32	count;
	__u32	forall;
	__u32	signaled;	/* out */
	__u32	first;		/* out: first signaled object's index */
};

/**
 * struct ntsync_qry_args - Query object state
 * @owner: Owning process ID (for mutex)
 * @count: Current count (for semaphore)
 * @signaled: Is object signaled?
 */
struct ntsync_qry_args {
	__u32	owner;
	__u32	count;
	__u32	signaled;
};

/* Ioctl definitions */
#define NTSYNC_IOC_CREATE_SEM		_IOWR(NTSYNC_IOC_BASE, 0x00, struct ntsync_sem_args)
#define NTSYNC_IOC_CREATE_MUTEX 	_IOWR(NTSYNC_IOC_BASE, 0x01, struct ntsync_mutex_args)
#define NTSYNC_IOC_CREATE_EVENT		_IOWR(NTSYNC_IOC_BASE, 0x02, struct ntsync_event_args)
#define NTSYNC_IOC_WAIT			_IOWR(NTSYNC_IOC_BASE, 0x03, struct ntsync_wait_args)
#define NTSYNC_IOC_QRY_SEM		_IOWR(NTSYNC_IOC_BASE, 0x04, struct ntsync_qry_args)
#define NTSYNC_IOC_QRY_MUTEX		_IOWR(NTSYNC_IOC_BASE, 0x05, struct ntsync_qry_args)
#define NTSYNC_IOC_QRY_EVENT		_IOWR(NTSYNC_IOC_BASE, 0x06, struct ntsync_qry_args)
#define NTSYNC_IOC_DELETE		_IO(NTSYNC_IOC_BASE, 0x07)

/* Special timeout values */
#define NTSYNC_INFINITE		(~0ULL)
#define NTSYNC_REALTIME		NTSYNC_WAIT_REALTIME

#endif /* _UAPI_LINUX_NTSYNC_H */