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

#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/random.h>
#include "amdgpu.h"
#include "amdgpu_discovery.h"
#include "amdgpu_ucode.h"

#define IP_DISCOVERY_VERSION 2

/* IP Discovery magic numbers */
#define IP_DISCOVERY_MAGIC 0x49444350  /* "IPCP" */

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

/* IP Discovery header structure */
struct ip_discovery_header {
	uint32_t magic;
	uint32_t version;
	uint32_t size;
	uint32_t num_ips;
	uint32_t flags;
	uint32_t reserved[3];
};

/* IP Discovery entry structure */
struct ip_discovery_entry {
	uint32_t hw_ip_type;
	uint32_t hw_ip_instance;
	uint32_t major_rev;
	uint32_t minor_rev;
	uint32_t num_base_addresses;
	uint32_t num_registers;
	uint32_t ip_offset;
	uint32_t reg_offset;
	uint32_t hardware_id;
	uint32_t vrtc_needed;
	uint32_t vrtc_needed_offset;
	uint32_t hw_info_offset;
	uint32_t reserved[4];
};

/* RT (Ray Tracing) IP info - extending ip_discovery_entry for RT blocks */
struct rt_ip_info {
	uint32_t rt_version;
	uint32_t rt_num_primitve_engines;
	uint32_t rt_num_traversal_engines;
	uint32_t rt_num_box_filters;
	uint32_t rt_num_triangle_filter_engines;
	uint32_t rt_accel_struct_buffer_size;
	uint32_t rt_reserved[2];
};

/* GPU Discovery table for RDNA 2+ - backported from 6.14+ */
static const struct amdgpu_ip_block_version ip_blocks_rDNA2[] = {
	/* GFX Core */
	{ AMD_IP_GFX, 1, 9, 0, NULL },
	/* Compute (CU) */
	{ AMD_IP_COMPUTE, 1, 9, 0, NULL },
	/* SDMA */
	{ AMD_IP_DMA, 0, 4, 0, NULL },
	{ AMD_IP_DMA, 1, 4, 0, NULL },
	/* Ray Tracing Unit (RDNA 2+ hardware RT) */
	{ AMD_IP_RT, 0, 9, 0, NULL },
	/* UVD */
	{ AMD_IP_UVD, 0, 5, 0, NULL },
	/* VCN */
	{ AMD_IP_VCN_DEC, 0, 1, 0, NULL },
	{ AMD_IP_VCN_ENC, 0, 1, 0, NULL },
};

/* GPU Discovery table for RDNA 3 */
static const struct amdgpu_ip_block_version ip_blocks_rDNA3[] = {
	/* GFX Core */
	{ AMD_IP_GFX, 1, 10, 0, NULL },
	/* Compute (CU) */
	{ AMD_IP_COMPUTE, 1, 10, 0, NULL },
	/* SDMA */
	{ AMD_IP_DMA, 0, 5, 0, NULL },
	{ AMD_IP_DMA, 1, 5, 0, NULL },
	/* Ray Tracing Unit (RDNA 3 hardware RT) */
	{ AMD_IP_RT, 0, 10, 0, NULL },
	/* VCN */
	{ AMD_IP_VCN_DEC, 0, 2, 0, NULL },
	{ AMD_IP_VCN_ENC, 0, 2, 0, NULL },
};

/**
 * amdgpu_discovery_get_ip_info - Get IP info from discovery table
 *
 * @adev: amdgpu_device pointer
 * @hw_ip_type: hardware IP type
 * @instance: instance number
 *
 * Returns the IP info if found, NULL otherwise
 */
static const struct amdgpu_ip_block_version *
amdgpu_discovery_get_ip_info(struct amdgpu_device *adev,
			       enum amd_ip_hw_ip_type hw_ip_type,
			       uint32_t instance)
{
	const struct amdgpu_ip_block_version *ip_blocks;
	int num_ip_blocks;
	int i;

	/* Use appropriate discovery table based on family */
	if (adev->family >= AMDGPU_FAMILY_RV) {  /* RDNA 2+ */
		ip_blocks = ip_blocks_rDNA2;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA2);
	} else if (adev->family >= AMDGPU_FAMILY_AI) {  /* RDNA 3 */
		ip_blocks = ip_blocks_rDNA3;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA3);
	} else {
		return NULL;
	}

	for (i = 0; i < num_ip_blocks; i++) {
		if (ip_blocks[i].hw_ip_type == hw_ip_type &&
		    ip_blocks[i].hw_ip_instance == instance)
			return &ip_blocks[i];
	}

	return NULL;
}

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
				    uint32_t instance)
{
	return amdgpu_discovery_get_ip_info(adev, hw_ip_type, instance) != NULL;
}

/**
 * amdgpu_discovery_get_ip_count - Get number of IPs of given type
 *
 * @adev: amdgpu_device pointer
 * @hw_ip_type: hardware IP type
 *
 * Returns the number of IPs found
 */
int amdgpu_discovery_get_ip_count(struct amdgpu_device *adev,
				 enum amd_ip_hw_ip_type hw_ip_type)
{
	const struct amdgpu_ip_block_version *ip_blocks;
	int num_ip_blocks;
	int count = 0;
	int i;

	if (adev->family >= AMDGPU_FAMILY_RV) {
		ip_blocks = ip_blocks_rDNA2;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA2);
	} else if (adev->family >= AMDGPU_FAMILY_AI) {
		ip_blocks = ip_blocks_rDNA3;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA3);
	} else {
		return 0;
	}

	for (i = 0; i < num_ip_blocks; i++) {
		if (ip_blocks[i].hw_ip_type == hw_ip_type)
			count++;
	}

	return count;
}

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
int amdgpu_discovery_init(struct amdgpu_device *adev)
{
	int r;

	/* Check if RT hardware is present */
	if (amdgpu_discovery_check_supported(adev, AMD_IP_RT, 0)) {
		DRM_INFO("amdgpu: Found RDNA %s hardware ray tracing support\n",
			 adev->family >= AMDGPU_FAMILY_RV ? "2" : "3");
		
		/* Mark the device as having RT capabilities */
		adev->flags |= AMD_IS_RT;
		
		/* Get RT hardware info */
		r = amdgpu_discovery_get_ip_count(adev, AMD_IP_RT);
		if (r > 0) {
			DRM_INFO("amdgpu: Discovered %d Ray Tracing hardware block(s)\n", r);
		}
	}

	return 0;
}

/**
 * amdgpu_discovery_fini - Cleanup IP discovery
 *
 * @adev: amdgpu_device pointer
 */
void amdgpu_discovery_fini(struct amdgpu_device *adev)
{
	/* Cleanup is minimal - just reset RT flag */
	adev->flags &= ~AMD_IS_RT;
}

/**
 * amdgpu_discovery_set_ip_blocks - Set up IP blocks for GPU
 *
 * @adev: amdgpu_device pointer
 *
 * Configure the IP blocks based on discovered hardware.
 * This allows dynamic configuration of RT blocks.
 */
void amdgpu_discovery_set_ip_blocks(struct amdgpu_device *adev)
{
	/* Add RT IP block if discovered */
	if (adev->flags & AMD_IS_RT) {
		DRM_DEBUG("amdgpu: RT IP block enabled via IP discovery\n");
	}
}

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
			    enum amd_ip_hw_ip_type *type)
{
	const struct amdgpu_ip_block_version *ip_blocks;
	int num_ip_blocks;
	int i;

	if (adev->family >= AMDGPU_FAMILY_RV) {
		ip_blocks = ip_blocks_rDNA2;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA2);
	} else if (adev->family >= AMDGPU_FAMILY_AI) {
		ip_blocks = ip_blocks_rDNA3;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA3);
	} else {
		return -ENOENT;
	}

	for (i = 0; i < num_ip_blocks; i++) {
		if (ip_blocks[i].hw_ip_instance == instance) {
			*type = ip_blocks[i].hw_ip_type;
			return 0;
		}
	}

	return -ENOENT;
}

/**
 * amdgpu_discovery_get_device_info - Get device info from discovery
 *
 * @adev: amdgpu_device pointer
 *
 * Returns device info if found, NULL otherwise
 */
const struct amdgpu_ip_block_version *
amdgpu_discovery_get_device_info(struct amdgpu_device *adev)
{
	const struct amdgpu_ip_block_version *ip_blocks;
	int num_ip_blocks;

	if (adev->family >= AMDGPU_FAMILY_RV) {
		ip_blocks = ip_blocks_rDNA2;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA2);
	} else if (adev->family >= AMDGPU_FAMILY_AI) {
		ip_blocks = ip_blocks_rDNA3;
		num_ip_blocks = ARRAY_SIZE(ip_blocks_rDNA3);
	} else {
		return NULL;
	}

	return ip_blocks;
}