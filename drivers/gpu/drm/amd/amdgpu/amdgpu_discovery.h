/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD(Radeon)
 */

#ifndef __AMDGPU_DISCOVERY_H__
#define __AMDGPU_DISCOVERY_H__

#include <linux/types.h>

/* Forward declaration */
struct amdgpu_device;

/* Hardware IP block types supported by RDNA 2+ */
enum amd_ip_hw_ip_type {
	AMD_IP_GFX = 0,
	AMD_IP_COMPUTE,
	AMD_IP_DMA,
	AMD_IP_UVD,
	AMD_IP_VCE,
	AMD_IP_UVD_ENC,
	AMD_IP_VCN_DEC,
	AMD_IP_VCN_ENC,
	AMD_IP_VCN_JPEG,
	AMD_IP_RT,           /* Ray Tracing hardware block (RDNA 2+) */
	AMD_IP_VIRTUAL,
	AMD_IP_NUM,
};

/* IP Discovery public interface */

/**
 * amdgpu_discovery_init - Initialize IP discovery
 *
 * @adev: amdgpu_device pointer
 *
 * Initialize the IP discovery framework for RDNA 2+ hardware.
 * This is essential because RDNA 2+ hardware is modular;
 * the driver must "discover" the RT blocks at boot rather than
 * relying on hardcoded tables.
 *
 * Returns 0 on success, negative error code otherwise
 */
int amdgpu_discovery_init(struct amdgpu_device *adev);

/**
 * amdgpu_discovery_fini - Cleanup IP discovery
 *
 * @adev: amdgpu_device pointer
 */
void amdgpu_discovery_fini(struct amdgpu_device *adev);

/**
 * amdgpu_discovery_set_ip_blocks - Set up IP blocks for GPU
 *
 * @adev: amdgpu_device pointer
 *
 * Configure the IP blocks based on discovered hardware.
 */
void amdgpu_discovery_set_ip_blocks(struct amdgpu_device *adev);

/**
 * amdgpu_discovery_check_supported - Check if IP is supported
 *
 * @adev: amdgpu_device pointer
 * @hw_ip_type: hardware IP type
 * @instance: instance number
 *
 * Returns true if supported, false otherwise
 */
bool amdgpu_discovery_check_supported(struct amdgpu_device *adev,
				    enum amd_ip_hw_ip_type hw_ip_type,
				    uint32_t instance);

/**
 * amdgpu_discovery_get_ip_count - Get number of IPs of given type
 *
 * @adev: amdgpu_device pointer
 * @hw_ip_type: hardware IP type
 *
 * Returns the number of IPs found
 */
int amdgpu_discovery_get_ip_count(struct amdgpu_device *adev,
				 enum amd_ip_hw_ip_type hw_ip_type);

/**
 * amdgpu_discovery_get_type - Get hardware IP type from instance
 *
 * @adev: amdgpu_device pointer
 * @instance: instance number
 * @type: output for hardware IP type
 *
 * Returns 0 on success, -ENOENT if not found
 */
int amdgpu_discovery_get_type(struct amdgpu_device *adev,
			    uint32_t instance,
			    enum amd_ip_hw_ip_type *type);

/**
 * amdgpu_discovery_get_device_info - Get device info from discovery
 *
 * @adev: amdgpu_device pointer
 *
 * Returns device info if found, NULL otherwise
 */
const struct amdgpu_ip_block_version *
amdgpu_discovery_get_device_info(struct amdgpu_device *adev);

#endif /* __AMDGPU_DISCOVERY_H__ */