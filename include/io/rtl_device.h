/*
 * Copyright (C) 2025 by arancormonk <180709949+arancormonk@users.noreply.github.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * @brief RTL-SDR device I/O public API.
 *
 * Declares the opaque `rtl_device` handle and functions for configuring and
 * streaming samples from an RTL-SDR, including optional offset tuning, gain
 * control, PPM correction, and asynchronous USB ingestion into an input ring.
 */
 
#pragma once

#include <pthread.h>
#include <rtl-sdr.h>
#include <stdint.h>
#include "runtime/input_ring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle for RTL-SDR device */
struct rtl_device;

/**
 * @brief Create and initialize an RTL-SDR device.
 *
 * @param dev_index Device index to open.
 * @param input_ring Pointer to input ring for USB data.
 * @param combine_rotate_enabled Whether to use combined rotate+widen when offset tuning is disabled.
 * @return Pointer to rtl_device handle, or NULL on failure.
 */
struct rtl_device* rtl_device_create(int dev_index, struct input_ring_state* input_ring, int combine_rotate_enabled);

/**
 * @brief Destroy an RTL-SDR device and free resources.
 *
 * @param dev Pointer to rtl_device handle.
 */
void rtl_device_destroy(struct rtl_device* dev);

/**
 * @brief Set device center frequency.
 *
 * @param dev RTL-SDR device handle.
 * @param frequency Frequency in Hz.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_frequency(struct rtl_device* dev, uint32_t frequency);

/**
 * @brief Set device sample rate.
 *
 * @param dev RTL-SDR device handle.
 * @param samp_rate Sample rate in Hz.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_sample_rate(struct rtl_device* dev, uint32_t samp_rate);

/**
 * @brief Set tuner gain mode and value.
 *
 * @param dev RTL-SDR device handle.
 * @param gain Gain in tenths of dB, or AUTO_GAIN for automatic.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_gain(struct rtl_device* dev, int gain);

/**
 * @brief Set frequency correction (PPM error).
 *
 * @param dev RTL-SDR device handle.
 * @param ppm_error PPM correction value.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_ppm(struct rtl_device* dev, int ppm_error);

/**
 * @brief Set direct sampling mode.
 *
 * @param dev RTL-SDR device handle.
 * @param on 1 to enable, 0 to disable.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_direct_sampling(struct rtl_device* dev, int on);

/**
 * @brief Set offset tuning mode.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success, negative on failure.
 */
int rtl_device_set_offset_tuning(struct rtl_device* dev);

/**
 * @brief Reset device buffer.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success, negative on failure.
 */
int rtl_device_reset_buffer(struct rtl_device* dev);

/**
 * @brief Start asynchronous reading from the device.
 *
 * @param dev RTL-SDR device handle.
 * @param buf_len Buffer length for async read.
 * @return 0 on success, negative on failure.
 */
int rtl_device_start_async(struct rtl_device* dev, uint32_t buf_len);

/**
 * @brief Stop asynchronous reading and join the device thread.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success, negative on failure.
 */
int rtl_device_stop_async(struct rtl_device* dev);

/**
 * @brief Mute the device for a specified number of samples.
 *
 * @param dev RTL-SDR device handle.
 * @param samples Number of samples to mute.
 */
void rtl_device_mute(struct rtl_device* dev, int samples);

#ifdef __cplusplus
}
#endif
