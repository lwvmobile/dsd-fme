/*
 * SIMD Widening and Rotation Header
 *
 * This header defines the interface for SIMD-accelerated conversion of
 * RTL-SDR USB data from unsigned 8-bit bytes to signed 16-bit integers,
 * with optional 90-degree IQ rotation. It uses runtime CPU feature detection
 * to select optimal implementations (AVX2, SSE2/SSSE3, NEON, or scalar fallback).
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

#ifndef DSD_FME_SIMD_WIDEN_H
#define DSD_FME_SIMD_WIDEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function pointer types for runtime dispatch */
typedef void (*dsd_fme_widen_fn)(const unsigned char*, int16_t*, uint32_t);
typedef void (*dsd_fme_widen_rot_fn)(const unsigned char*, int16_t*, uint32_t);

/**
 * Public wrapper that lazy-initializes runtime dispatch and widens u8 to s16
 * centered at 127.
 *
 * @param src Source buffer of unsigned bytes (I/Q interleaved).
 * @param dst Destination int16 buffer.
 * @param len Number of bytes in src to process.
 */
void widen_u8_to_s16_bias127(const unsigned char* src, int16_t* dst, uint32_t len);

/**
 * Public wrapper that lazy-initializes runtime dispatch and performs 90° IQ
 * rotation combined with widen u8→s16 centered at 127.
 *
 * @param src Source buffer of unsigned bytes (I/Q interleaved).
 * @param dst Destination int16 buffer.
 * @param len Number of bytes in src to process.
 */
void widen_rotate90_u8_to_s16_bias127(const unsigned char* src, int16_t* dst, uint32_t len);

/**
 * Scalar widening that subtracts 128 instead of 127.
 * Intended to pair with legacy byte-wise rotate_90(u8) which performs 255-x
 * negation so that overall effect equals correct centered negation (127-x).
 *
 * @param src Source buffer of unsigned bytes.
 * @param dst Destination int16 buffer.
 * @param len Number of bytes to process.
 */
void widen_u8_to_s16_bias128_scalar(const unsigned char* src, int16_t* dst, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* DSD_FME_SIMD_WIDEN_H */
