/*
 * SIMD Widening/Rotation Module
 *
 * This module provides efficient SIMD-accelerated widening of unsigned bytes
 * to signed 16-bit integers, with optional 90° IQ rotation. It uses runtime
 * CPU feature detection to select the optimal implementation (AVX2, SSE2/SSSE3,
 * NEON, or scalar fallback).
 *
 * This code was extracted from src/rtl_sdr_fm.cpp as part of the RTL-SDR FM
 * refactoring plan (Phase 1).
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
