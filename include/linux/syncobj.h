/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sync Object Timeline Support (Backport from Linux 5.10+)
 *
 * Hardware RT is heavily asynchronous. This provides the syncobj timeline
 * support needed to prevent GPU hangs when multiple RT shaders are in flight.
 */
#ifndef _SYNCOBJ_H
#define _SYNCOBJ_H

#include <linux/types.h>

/**
 * struct syncobj_export - Sync object export for userspace
 * @fd: file descriptor
 * @fence: fence pointer
 */
struct syncobj_export {
	int fd;
	void *fence;  /* struct dma_fence * */
};

/**
 * struct syncobj_timeline_info - Timeline information
 * @context: timeline context
 * @timeline_name: name of the timeline
 * @current_seqno: current sequence number
 */
struct syncobj_timeline_info {
	__u64 context;
	char timeline_name[32];
	__u32 current_seqno;
};

/* Syncobj create flags */
#define SYNCOBJ_CREATE_SIGNALED		0x1	/* Create in signaled state */
#define SYNCOBJ_CREATE_TIMELINE		0x2	/* Create timeline syncobj */

/**
 * syncobj_create - Create a sync object
 * @flags: creation flags
 * @fence: output fence (optional)
 *
 * Create a new sync object. Returns 0 on success, negative error code.
 */
int syncobj_create(__u32 flags, void **fence);

/**
 * syncobj_destroy - Destroy a sync object
 * @fence: fence pointer
 *
 * Destroy a sync object. Returns 0 on success, negative error code.
 */
int syncobj_destroy(void *fence);

/**
 * syncobj_signal - Signal a sync object
 * @fence: fence pointer
 *
 * Signal a sync object. Returns 0 on success, negative error code.
 */
int syncobj_signal(void *fence);

/**
 * syncobj_reset - Reset a sync object
 * @fence: fence pointer
 *
 * Reset a sync object to unsignaled state. Returns 0 on success.
 */
int syncobj_reset(void *fence);

/**
 * syncobj_wait - Wait for a sync object
 * @fence: fence pointer
 * @timeout: timeout in nanoseconds
 * @flags: wait flags
 *
 * Wait for a sync object to be signaled. Returns 0 on success.
 */
int syncobj_wait(void *fence, __u64 timeout, __u32 flags);

/**
 * syncobjfd_to_fence - Get fence from syncobj fd
 * @fd: file descriptor
 * @fence: output fence pointer
 *
 * Get a fence pointer from a syncobj file descriptor.
 * Returns 0 on success, negative error code.
 */
int syncobjfd_to_fence(int fd, void **fence);

/**
 * syncobj_handle_to_fence - Get fence from syncobj handle
 * @handle: syncobj handle
 * @fence: output fence pointer
 *
 * Get a fence pointer from a syncobj handle.
 * Returns 0 on success, negative error code.
 */
int syncobj_handle_to_fence(__u32 handle, void **fence);

/**
 * syncobj_export_to_fd - Export syncobj to fd
 * @fence: fence pointer
 * @fd: output file descriptor
 *
 * Export a sync object to a file descriptor.
 * Returns 0 on success, negative error code.
 */
int syncobj_export_to_fd(void *fence, int *fd);

/**
 * syncobj_get_timeline_info - Get timeline information
 * @fence: fence pointer
 * @info: output timeline info
 *
 * Get timeline information for a sync object.
 * Returns 0 on success, negative error code.
 */
int syncobj_get_timeline_info(void *fence, struct syncobj_timeline_info *info);

/**
 * syncobj_sync_packet - Sync packet for timeline
 * @fence: fence pointer
 * @cmd: packet command
 * @flags: packet flags
 *
 * Sync a packet for timeline-based synchronization.
 * Returns packet sequence number, negative error code.
 */
int syncobj_sync_packet(void *fence, __u32 cmd, __u32 flags);

#endif /* _SYNCOBJ_H */