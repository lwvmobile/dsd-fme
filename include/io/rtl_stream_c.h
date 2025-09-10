/*
 * C shim API for RtlSdrOrchestrator (RAII) to be used from C code
 *
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
int rtl_stream_create(const dsd_opts* opts, RtlSdrContext** out_ctx);
int rtl_stream_start(RtlSdrContext* ctx);
int rtl_stream_stop(RtlSdrContext* ctx);
int rtl_stream_destroy(RtlSdrContext* ctx);

/* Control */
int rtl_stream_tune(RtlSdrContext* ctx, uint32_t center_freq_hz);

/* I/O */
int rtl_stream_read(RtlSdrContext* ctx, int16_t* out, size_t count, int* out_got);
uint32_t rtl_stream_output_rate(const RtlSdrContext* ctx);

/* Optional helpers to mirror legacy API behavior */
void rtl_stream_clear_output(RtlSdrContext* ctx);
long rtl_stream_return_pwr(const RtlSdrContext* ctx);

#ifdef __cplusplus
}
#endif
