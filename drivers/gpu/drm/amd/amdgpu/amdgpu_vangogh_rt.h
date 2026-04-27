/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Van Gogh (Steam Deck) RT Power Management
 *
 * Provides RT-specific power and clock gating for the Steam Deck APU.
 * The RT core requires a "Wake" signal during IP Discovery or hardware remains dark.
 */
#ifndef _AMDGPU_VANGOGH_RT_PM_H
#define _AMDGPU_VANGOGH_RT_PM_H

#include <linux/types.h>

/* Van Gogh RT hardware identifiers */
#define AMDGPU_VANGOGH_RT_IP_VERSION	0x01
#define AMDGPU_VANGOGH_RT_ENGINE_COUNT	1

/* RT power states */
enum amdgpu_rt_power_state {
	AMDGPU_RT_POWER_DOWN = 0,
	AMDGPU_RT_POWER_UP = 1,
	AMDGPU_RT_POWER_ACTIVE = 2,
};

/* RT clock gating states */
enum amdgpu_rt_clock_state {
	AMDGPU_RT_CLOCK_OFF = 0,
	AMDGPU_RT_CLOCK_GATED = 1,
	AMDGPU_RT_CLOCK_ACTIVE = 2,
};

/**
 * amdgpu_vangogh_rt_pwr_init - Initialize RT power management for Van Gogh
 * @adev: amdgpu device pointer
 *
 * Called during device initialization to set up RT power rails.
 * Returns 0 on success, negative error code.
 */
int amdgpu_vangogh_rt_pwr_init(void *adev);

/**
 * amdgpu_vangogh_rt_pwr_fini - Cleanup RT power management
 * @adev: amdgpu device pointer
 *
 * Called during device cleanup.
 */
void amdgpu_vangogh_rt_pwr_fini(void *adev);

/**
 * amdgpu_vangogh_rt_set_power_state - Set RT power state
 * @adev: amdgpu device pointer
 * @state: power state to set
 *
 * Sets the RT core power state. Must be called before IP Discovery
 * if RT is supported to ensure the hardware is powered up.
 * Returns 0 on success, negative error code.
 */
int amdgpu_vangogh_rt_set_power_state(void *adev, enum amdgpu_rt_power_state state);

/**
 * amdgpu_vangogh_rt_set_clock_gating - Set RT clock gating
 * @adev: amdgpu device pointer
 * @state: clock gating state
 *
 * Controls RT core clock gating to save power when not in use.
 * Returns 0 on success, negative error code.
 */
int amdgpu_vangogh_rt_set_clock_gating(void *adev, enum amdgpu_rt_clock_state state);

/**
 * amdgpu_vangogh_rt_wake_hw - Send RT wake signal to hardware
 * @adev: amdgpu device pointer
 *
 * Sends the critical "Wake" signal to the RT core during IP Discovery.
 * This is required - without it the RT hardware remains dark even
 * if the driver is ready.
 * Returns 0 on success, negative error code.
 */
int amdgpu_vangogh_rt_wake_hw(void *adev);

/**
 * amdgpu_vangogh_rt_get_capabilities - Query RT hardware capabilities
 * @adev: amdgpu device pointer
 * @engine_count: output for number of RT engines
 * @rays_per_second: output for ray-triangle intersection throughput
 *
 * Queries the hardware-specific RT capabilities for the UAPI.
 * Returns 0 on success, negative error code.
 */
int amdgpu_vangogh_rt_get_capabilities(void *adev, unsigned int *engine_count,
				   unsigned long long *rays_per_second);

/**
 * amdgpu_vangogh_rt_is_supported - Check if RT is hardware supported
 * @adev: amdgpu device pointer
 *
 * Returns true if Van Gogh has hardware RT support.
 */
bool amdgpu_vangogh_rt_is_supported(void *adev);

#endif /* _AMDGPU_VANGOGH_RT_PM_H */