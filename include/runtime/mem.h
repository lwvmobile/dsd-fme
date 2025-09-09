/*
 * Runtime Memory Management Header
 *
 * This header provides aligned memory allocation utilities for DSP operations,
 * ensuring proper memory alignment for SIMD operations and cache efficiency.
 * It centralizes memory management to maintain ownership tracking and safe cleanup.
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

#ifdef __cplusplus
extern "C" {
#endif

/* Default alignment for hot DSP buffers */
#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

/* Allocate size bytes aligned to DSD_FME_ALIGN. Returns NULL on failure. */
void* dsd_fme_aligned_malloc(size_t size);

/* Free memory allocated by dsd_fme_aligned_malloc (or plain malloc fallback). */
void dsd_fme_aligned_free(void* ptr);

#ifdef __cplusplus
}
#endif
