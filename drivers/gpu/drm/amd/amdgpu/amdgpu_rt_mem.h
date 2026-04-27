/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RT (Ray Tracing) Memory Management for RDNA 2/3
 *
 * Provides high-priority memory allocation for BVH (Bounding Volume Hierarchy)
 * buffers to prevent the RT hardware from stalling.
 */
#ifndef _AMDGPU_RT_MEM_H
#define _AMDGPU_RT_MEM_H

#include <linux/types.h>

/* RT buffer allocation flags - must be kept in sync with drm_amdgpu.h */
#define AMDGPU_GEM_CREATE_RT_HIGH_PRIORITY	(1 << 31)
#define AMDGPU_GEM_CREATE_RT_BVH_BUFFER	(1 << 30)
#define AMDGPU_GEM_CREATE_RT_ACCEL_BUFFER	(1 << 29)

/* RT memory priority levels */
enum amdgpu_rt_mem_priority {
	AMDGPU_RT_PRIORITY_NORMAL = 0,
	AMDGPU_RT_PRIORITY_HIGH = 1,
	AMDGPU_RT_PRIORITY_CRITICAL = 2,
};

/**
 * amdgpu_rt_mem_init - Initialize RT memory management
 *
 * Called during device initialization to set up any RT-specific memory pools.
 * Returns 0 on success, negative error code.
 */
int amdgpu_rt_mem_init(void);

/**
 * amdgpu_rt_mem_fini - Cleanup RT memory management
 *
 * Called during device cleanup.
 */
void amdgpu_rt_mem_fini(void);

/**
 * amdgpu_rt_mem_priority - Get memory priority for RT buffer
 * @size: buffer size in bytes
 * @flags: buffer creation flags
 *
 * Returns the appropriate memory priority based on buffer type.
 */
enum amdgpu_rt_mem_priority amdgpu_rt_mem_priority(u64 size, u64 flags);

/**
 * amdgpu_rt_mem_is_rt_buffer - Check if buffer is RT-related
 * @flags: buffer creation flags
 *
 * Returns true if the buffer is for ray tracing.
 */
bool amdgpu_rt_mem_is_rt_buffer(u64 flags);

/**
 * amdgpu_rt_mem_pin_bvh - Pin BVH buffer in VRAM
 * @bo: buffer object pointer
 *
 * Ensure BVH buffer is in high-priority VRAM to prevent RT stalls.
 * Returns 0 on success, negative error code.
 */
int amdgpu_rt_mem_pin_bvh(void *bo);

/**
 * amdgpu_rt_mem_unpin_bvh - Unpin BVH buffer
 * @bo: buffer object pointer
 *
 * Release high-priority pinning for BVH buffer.
 */
void amdgpu_rt_mem_unpin_bvh(void *bo);

/**
 * amdgpu_rt_mem_get_bvh_alignment - Get required BVH alignment
 *
 * Returns the alignment requirement for BVH buffers (typically 256KB).
 */
u64 amdgpu_rt_mem_get_bvh_alignment(void);

/**
 * amdgpu_rt_mem_get_accel_alignment - Get required acceleration buffer alignment
 *
 * Returns the alignment requirement for acceleration buffers (typically 4KB).
 */
u64 amdgpu_rt_mem_get_accel_alignment(void);

#endif /* _AMDGPU_RT_MEM_H */