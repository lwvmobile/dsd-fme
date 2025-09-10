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
 * @brief C shim header for the RTL-SDR orchestrator.
 *
 * Declares a minimal C API mirroring lifecycle, tuning, and I/O operations
 * of the C++ `RtlSdrOrchestrator`, for consumption by C translation units.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dsd.h"

/* Opaque stream context */
typedef struct RtlSdrContext RtlSdrContext;

/* Lifecycle */
/**
 * @brief Create a new RTL-SDR stream context from options.
 * @param opts Decoder options snapshot used to configure the stream. Must not be NULL.
 * @param out_ctx [out] On success, receives an opaque context pointer.
 * @return 0 on success; otherwise <0 on error.
 */
int rtl_stream_create(const dsd_opts* opts, RtlSdrContext** out_ctx);
/**
 * @brief Start the stream threads and device I/O.
 * @param ctx Stream context created by rtl_stream_create().
 * @return 0 on success; otherwise <0 on error.
 */
int rtl_stream_start(RtlSdrContext* ctx);
/**
 * @brief Stop the stream and cleanup resources associated with the run.
 * Safe to call multiple times; subsequent calls are no-ops.
 * @param ctx Stream context created by rtl_stream_create().
 * @return 0 on success; otherwise <0 on error.
 */
int rtl_stream_stop(RtlSdrContext* ctx);
/**
 * @brief Destroy the stream context and free all associated resources.
 * If the stream is running, it is stopped before destruction.
 * @param ctx Stream context to destroy. May be NULL.
 * @return 0 always.
 */
int rtl_stream_destroy(RtlSdrContext* ctx);

/* Control */
/**
 * @brief Tune to a new center frequency.
 * @param ctx Stream context.
 * @param center_freq_hz New center frequency in Hz.
 * @return 0 on success; otherwise <0 on error.
 */
int rtl_stream_tune(RtlSdrContext* ctx, uint32_t center_freq_hz);

/* I/O */
/**
 * @brief Read up to `count` interleaved audio samples into `out`.
 * @param ctx Stream context.
 * @param out Destination buffer for samples. Must not be NULL.
 * @param count Maximum number of samples to read.
 * @param out_got [out] Set to the number of samples actually read.
 * @return 0 on success; otherwise <0 on error (e.g., shutdown).
 */
int rtl_stream_read(RtlSdrContext* ctx, int16_t* out, size_t count, int* out_got);
/**
 * @brief Get the current output sample rate in Hz.
 * @param ctx Stream context.
 * @return Output sample rate in Hz; returns 0 if `ctx` is invalid.
 */
uint32_t rtl_stream_output_rate(const RtlSdrContext* ctx);

/* Optional helpers to mirror legacy API behavior */
/**
 * @brief Clear the output ring buffer and wake any waiting producer.
 * Mirrors the legacy behavior. The `ctx` parameter is currently ignored.
 * @param ctx Stream context (unused).
 */
void rtl_stream_clear_output(RtlSdrContext* ctx);
/**
 * @brief Return mean power approximation (RMS^2 proxy) for soft squelch.
 * The computation uses a small fixed sample window and mirrors the legacy implementation.
 * @param ctx Stream context (unused).
 * @return Mean power value (approximate RMS squared).
 */
long rtl_stream_return_pwr(const RtlSdrContext* ctx);

#ifdef __cplusplus
}
#endif
