/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <atomic>
#include <rtl-sdr.h>
#include "dsd.h"

/* Optional SIMD intrinsics for USB byte->int16 widening */
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

/* Runtime CPU feature detection headers */
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#if defined(__linux__)
#include <sys/auxv.h>
#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif
#endif

/*
 * Environment variables (runtime configuration)
 * Set in your shell before launching dsd-fme, e.g.:
 *   $ export DSD_FME_RESAMP=48000
 *   $ export DSD_FME_FLL=1 DSD_FME_FLL_ALPHA=150 DSD_FME_FLL_BETA=15
 *   $ export DSD_FME_TED=1 DSD_FME_TED_SPS=10 DSD_FME_TED_GAIN=96
 *   $ export DSD_FME_AUDIO_LPF=3000
 *   $ ./build/dsd-fme ...
 *
 * Realtime scheduling and CPU affinity
 * - DSD_FME_RT_SCHED
 *     Enable best-effort realtime scheduling (SCHED_FIFO). Requires CAP_SYS_NICE or root.
 *     Values: "1" to enable, unset/other to disable. Default: disabled.
 * - DSD_FME_RT_PRIO_USB | DSD_FME_RT_PRIO_DONGLE | DSD_FME_RT_PRIO_DEMOD
 *     Optional per-thread priorities (1..99, clamped to system limits). Used only if RT_SCHED=1.
 *     Example: export DSD_FME_RT_PRIO_DEMOD=85
 * - DSD_FME_CPU_USB | DSD_FME_CPU_DONGLE | DSD_FME_CPU_DEMOD
 *     Optional CPU core pinning for each thread. Integer CPU id (>=0). Example: export DSD_FME_CPU_DEMOD=2
 *
 * Frontend/decimation/upsampling
 * - DSD_FME_HB_DECIM
 *     Use half-band FIR decimator cascade (fast, good response) instead of legacy CIC-like path.
 *     Values: 1 enable, 0 disable. Default: 1 (enabled).
 * - DSD_FME_COMBINE_ROT
 *     Combine 90° IQ rotation with USB byte→int16 widening in one pass when offset tuning is off.
 *     Values: 1 enable, 0 disable. Default: 1 (enabled).
 * - DSD_FME_UPSAMPLE_FP
 *     Use fixed-point arithmetic in legacy linear upsampler for lower CPU/divisions.
 *     Values: 1 enable, 0 disable. Default: 1 (enabled).
 *
 * Rational resampler (polyphase upfirdn L/M)
 * - DSD_FME_RESAMP
 *     Target output sample rate in Hz. Enables L/M resampler when set.
 *     Values: "off" or "0" to disable; integer Hz (e.g., 48000) to enable. Default: 48000 (enabled).
 *
 * Residual CFO frequency-locked loop (FLL)
 * - DSD_FME_FLL
 *     Enable residual carrier frequency correction.
 *     Values: "1" or unset to enable; other values disable. Default: enabled.
 * - DSD_FME_FLL_LUT
 *     Use higher-quality quarter-wave sine LUT mixer for FLL rotation.
 *     Values: 1 enable, 0/empty disable. Default: 0 (disabled; fast piecewise approx).
 * - DSD_FME_FLL_ALPHA, DSD_FME_FLL_BETA
 *     Proportional and integral gains (Q15 fixed-point, ~value/32768). Typical small values.
 *     Defaults: ALPHA=100 (~0.003), BETA=10 (~0.0003). May be adjusted for digital modes if not set.
 *
 * Gardner timing error detector (TED)
 * - DSD_FME_TED
 *     Enable lightweight fractional-delay timing correction. Generally off for analog FM.
 *     Values: 1 enable, else disabled. Default: 0 (disabled). For certain digital modes, defaults are adjusted
 *     only if envs are not provided (still off unless forced via DSD_FME_TED=1).
 * - DSD_FME_TED_SPS
 *     Nominal samples-per-symbol (integer). If unset and a digital mode is active, it is derived from output rate.
 *     Default: 10.
 * - DSD_FME_TED_GAIN
 *     Small loop gain (Q20). Default: 64; for common digital modes may default to 96 when not provided.
 * - DSD_FME_TED_FORCE
 *     Force TED to run for FM/C4FM paths where it is normally skipped. Values: 1 enable, else disabled. Default: 0.
 *
 * Audio processing
 * - DSD_FME_DEEMPH
 *     Post-demod deemphasis time constant. Applies only when the active demod preset enables deemphasis.
 *     Values: "75" (75µs, default), "50" (50µs), "nfm" (~750µs), "off" (disable).
 * - DSD_FME_AUDIO_LPF
 *     Optional one-pole low-pass filter after demod. Approximate cutoff in Hz.
 *     Values: "off" or "0" to disable; integer (e.g., 3000, 5000) to enable. Default: off.
 *
 * Intra-block multithreading
 * - DSD_FME_MT
 *     Enable a minimal 2-thread worker pool for certain CPU-heavy inner loops.
 *     Values: 1 enable, else disabled. Default: 0 (disabled).
 */

#define DEFAULT_SAMPLE_RATE		48000
#define DEFAULT_BUF_LENGTH		(1 * 16384)
#define MAXIMUM_OVERSAMPLE		16
#define MAXIMUM_BUF_LENGTH		(MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define AUTO_GAIN			-100
#define BUFFER_DUMP		4096

#define FREQUENCIES_LIMIT		  1000

/* Clamp for bandwidth upsampling multiplier to avoid extreme expansion */
#define MAX_BANDWIDTH_MULTIPLIER 8

static int lcm_post[17] = {1,1,1,3,1,5,3,7,1,9,5,11,3,13,7,15,1};
static int ACTUAL_BUF_LENGTH;

static const double kPi = 3.14159265358979323846;

#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_PRAGMA(x) _Pragma(#x)
#define DSD_FME_IVDEP DSD_FME_PRAGMA(GCC ivdep)
/**
 * Hint that a pointer is aligned to a compile-time boundary for vectorization.
 *
 * This is a lightweight wrapper over compiler intrinsics to improve
 * auto-vectorization by promising the compiler that the pointer meets the
 * specified alignment. Use with care and only when the alignment guarantee
 * is actually met.
 *
 * @tparam T Element type of the pointer.
 * @param p  Pointer to memory that is at least `align_unused` aligned.
 * @param align_unused Alignment in bytes (ignored at runtime; for readability).
 * @return Pointer `p` with alignment assumption applied.
 */
template <typename T>
static inline T* assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return (T*)__builtin_assume_aligned(p, 64);
}
#else
#define DSD_FME_IVDEP
/**
 * See aligned variant: noop fallback when compiler does not support alignment
 * assumptions.
 * @tparam T Element type of the pointer.
 * @param p  Pointer to return as-is.
 * @param align_unused Unused parameter for signature compatibility.
 * @return Pointer `p` unchanged.
 */
template <typename T>
static inline T* assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return p;
}
#endif
#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

/* Compiler-friendly restrict qualifier */
#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_RESTRICT __restrict__
#else
#define DSD_FME_RESTRICT
#endif

static int *atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;
static pthread_once_t atan_lut_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t atan_lut_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Optional quarter-wave sine LUT for FLL rotator (Q15). */
static int16_t fll_qsine_q15_lut[1025]; /* 0..pi/2 in 1024 steps, +1 guard for exact pi/2 */
static pthread_once_t fll_lut_once = PTHREAD_ONCE_INIT;
static int fll_lut_enabled = 0; /* DSD_FME_FLL_LUT (0 default: use fast approx) */
/* Debug/compat toggles via env */
static int combine_rotate_enabled = 1;     /* DSD_FME_COMBINE_ROT (1 default) */
static int upsample_fixedpoint_enabled = 1;/* DSD_FME_UPSAMPLE_FP (1 default) */

/* Forward declaration for runtime SIMD dispatch initializer */
static void dsd_fme_init_runtime_dispatch(void);
static void dsd_fme_init_runtime_dispatch_once(void);
static pthread_once_t dsd_fme_dispatch_once_control = PTHREAD_ONCE_INIT;

typedef void (*dsd_fme_widen_fn)(const unsigned char*, int16_t*, uint32_t);
typedef void (*dsd_fme_widen_rot_fn)(const unsigned char*, int16_t*, uint32_t);
static dsd_fme_widen_fn     g_widen_impl = NULL;
static dsd_fme_widen_rot_fn g_widen_rot_impl = NULL;

/**
 * Public wrapper that lazy-initializes runtime dispatch and widens u8 to s16
 * centered at 127.
 *
 * @param src Source buffer of unsigned bytes (I/Q interleaved).
 * @param dst Destination int16 buffer.
 * @param len Number of bytes in src to process.
 */
static inline void widen_u8_to_s16_bias127(const unsigned char * DSD_FME_RESTRICT src,
    int16_t * DSD_FME_RESTRICT dst, uint32_t len)
{
    if (!g_widen_impl) dsd_fme_init_runtime_dispatch();
    g_widen_impl(src, dst, len);
}

/**
 * Public wrapper that lazy-initializes runtime dispatch and performs 90° IQ
 * rotation combined with widen u8→s16 centered at 127.
 *
 * @param src Source buffer of unsigned bytes (I/Q interleaved).
 * @param dst Destination int16 buffer.
 * @param len Number of bytes in src to process.
 */
static inline void widen_rotate90_u8_to_s16_bias127(const unsigned char * DSD_FME_RESTRICT src,
    int16_t * DSD_FME_RESTRICT dst, uint32_t len)
{
    if (!g_widen_rot_impl) dsd_fme_init_runtime_dispatch();
    g_widen_rot_impl(src, dst, len);
}

/**
 * Scalar fallback: widen u8 to s16 centered at 127.
 *
 * @param src Source buffer of unsigned bytes.
 * @param dst Destination int16 buffer.
 * @param len Number of bytes to process.
 */
static inline void widen_u8_to_s16_bias127_scalar(const unsigned char * DSD_FME_RESTRICT src,
    int16_t * DSD_FME_RESTRICT dst, uint32_t len)
{
    uint32_t i = 0;
    /* Scalar conversion: (u8 - 127) -> s16 */
    for (; i < len; i++) {
        dst[i] = (int16_t)src[i] - 127;
    }
}

/**
 * Scalar widening that subtracts 128 instead of 127.
 * Intended to pair with legacy byte-wise rotate_90(u8) which performs 255-x
 * negation so that overall effect equals correct centered negation (127-x).
 *
 * @param src Source buffer of unsigned bytes.
 * @param dst Destination int16 buffer.
 * @param len Number of bytes to process.
 */
static inline void widen_u8_to_s16_bias128_scalar(const unsigned char * DSD_FME_RESTRICT src,
    int16_t * DSD_FME_RESTRICT dst, uint32_t len)
{
    uint32_t i = 0;
    for (; i < len; i++) {
        dst[i] = (int16_t)src[i] - 128;
    }
}

/**
 * Combined 90° rotation (1, j, -1, -j) + widen (u8→s16 centered at 127).
 * Processes 4 IQ samples per iteration to avoid branches.
 *
 * @param src Source buffer of unsigned bytes (I/Q interleaved).
 * @param dst Destination int16 buffer.
 * @param len Number of bytes in src to process.
 */
static inline void widen_rotate90_u8_to_s16_bias127_scalar(const unsigned char * DSD_FME_RESTRICT src,
    int16_t * DSD_FME_RESTRICT dst, uint32_t len)
{
    uint32_t i = 0;
    for (; i + 8 <= len; i += 8) {
        int16_t i0 = (int16_t)src[i + 0] - 127;
        int16_t q0 = (int16_t)src[i + 1] - 127;
        dst[i + 0] = i0;
        dst[i + 1] = q0;

        int16_t i1 = (int16_t)src[i + 2] - 127;
        int16_t q1 = (int16_t)src[i + 3] - 127;
        dst[i + 2] = (int16_t)(-q1);
        dst[i + 3] = i1;

        int16_t i2 = (int16_t)src[i + 4] - 127;
        int16_t q2 = (int16_t)src[i + 5] - 127;
        dst[i + 4] = (int16_t)(-i2);
        dst[i + 5] = (int16_t)(-q2);

        int16_t i3 = (int16_t)src[i + 6] - 127;
        int16_t q3 = (int16_t)src[i + 7] - 127;
        dst[i + 6] = q3;
        dst[i + 7] = (int16_t)(-i3);
    }
    /* Tail: apply rotation pattern for remaining up to 6 samples */
    if (i < len) {
        uint32_t base = i;
        uint32_t rem = len - base;
        if (rem >= 2) {
            int16_t i0 = (int16_t)src[base + 0] - 127;
            int16_t q0 = (int16_t)src[base + 1] - 127;
            dst[base + 0] = i0;
            dst[base + 1] = q0;
        }
        if (rem >= 4) {
            int16_t i1 = (int16_t)src[base + 2] - 127;
            int16_t q1 = (int16_t)src[base + 3] - 127;
            dst[base + 2] = (int16_t)(-q1);
            dst[base + 3] = i1;
        }
        if (rem >= 6) {
            int16_t i2 = (int16_t)src[base + 4] - 127;
            int16_t q2 = (int16_t)src[base + 5] - 127;
            dst[base + 4] = (int16_t)(-i2);
            dst[base + 5] = (int16_t)(-q2);
        }
    }
}

#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_TARGET_ATTR(x) __attribute__((target(x)))
#else
#define DSD_FME_TARGET_ATTR(x)
#endif

#if defined(__x86_64__) || defined(__i386__)
/* AVX2 specializations */
/**
 * AVX2: widen unsigned bytes to signed 16-bit centered at 127.
 * @param src Source u8 buffer.
 * @param dst Destination s16 buffer.
 * @param len Number of bytes to process.
 */
static void DSD_FME_TARGET_ATTR("avx2") widen_u8_to_s16_bias127_avx2(const unsigned char *src, int16_t *dst, uint32_t len)
{
    uint32_t i = 0;
    const __m256i bias256 = _mm256_set1_epi16(127);
    for (; i + 32 <= len; i += 32) {
        __m128i b0 = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i b1 = _mm_loadu_si128((const __m128i*)(src + i + 16));
        __m256i lo = _mm256_cvtepu8_epi16(b0);
        __m256i hi = _mm256_cvtepu8_epi16(b1);
        lo = _mm256_sub_epi16(lo, bias256);
        hi = _mm256_sub_epi16(hi, bias256);
        _mm256_storeu_si256((__m256i*)(dst + i), lo);
        _mm256_storeu_si256((__m256i*)(dst + i + 16), hi);
    }
    for (; i < len; i++) dst[i] = (int16_t)src[i] - 127;
}

/**
 * AVX2: rotate (1,j,-1,-j) interleaved IQ and widen u8→s16 centered at 127.
 * Tail elements are handled by a scalar helper to preserve the rotation pattern.
 * @param src Source u8 buffer (I/Q interleaved).
 * @param dst Destination s16 buffer.
 * @param len Number of bytes to process.
 */
static void DSD_FME_TARGET_ATTR("avx2") widen_rotate90_u8_to_s16_bias127_avx2(const unsigned char *src, int16_t *dst, uint32_t len)
{
    const __m256i shuffle = _mm256_setr_epi8(
        0, 1, 3, 2, 4, 5, 7, 6,  8, 9,11,10,12,13,15,14,
        0, 1, 3, 2, 4, 5, 7, 6,  8, 9,11,10,12,13,15,14);
    const __m256i mask_sel = _mm256_setr_epi16(
        0x0000,0x0000,0xFFFF,0x0000,0xFFFF,0xFFFF,0x0000,0xFFFF,
        0x0000,0x0000,0xFFFF,0x0000,0xFFFF,0xFFFF,0x0000,0xFFFF);
    const __m256i c127 = _mm256_set1_epi16(127);
    uint32_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i v8 = _mm256_loadu_si256((const __m256i*)(src + i));
        __m256i sh = _mm256_shuffle_epi8(v8, shuffle);
        __m128i sh_lo = _mm256_castsi256_si128(sh);
        __m128i sh_hi = _mm256_extracti128_si256(sh, 1);
        __m256i v16_lo = _mm256_cvtepu8_epi16(sh_lo);
        __m256i v16_hi = _mm256_cvtepu8_epi16(sh_hi);
        __m256i bs_lo = _mm256_sub_epi16(v16_lo, c127);
        __m256i bm_lo = _mm256_sub_epi16(c127,  v16_lo);
        __m256i bs_hi = _mm256_sub_epi16(v16_hi, c127);
        __m256i bm_hi = _mm256_sub_epi16(c127,  v16_hi);
        __m256i out_lo = _mm256_blendv_epi8(bs_lo, bm_lo, mask_sel);
        __m256i out_hi = _mm256_blendv_epi8(bs_hi, bm_hi, mask_sel);
        _mm256_storeu_si256((__m256i*)(dst + i), out_lo);
        _mm256_storeu_si256((__m256i*)(dst + i + 16), out_hi);
    }
    /* Tail: preserve rotation via scalar helper */
    if (i < len) {
        widen_rotate90_u8_to_s16_bias127_scalar(src + i, dst + i, len - i);
    }
}

/**
 * SSE2: widen unsigned bytes to signed 16-bit centered at 127.
 */
static void DSD_FME_TARGET_ATTR("sse2") widen_u8_to_s16_bias127_sse2(const unsigned char *src, int16_t *dst, uint32_t len)
{
    uint32_t i = 0;
    const __m128i bias = _mm_set1_epi16(127);
    const __m128i zero = _mm_setzero_si128();
    for (; i + 16 <= len; i += 16) {
        __m128i b = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i lo = _mm_unpacklo_epi8(b, zero);
        __m128i hi = _mm_unpackhi_epi8(b, zero);
        lo = _mm_sub_epi16(lo, bias);
        hi = _mm_sub_epi16(hi, bias);
        _mm_storeu_si128((__m128i*)(dst + i), lo);
        _mm_storeu_si128((__m128i*)(dst + i + 8), hi);
    }
    for (; i < len; i++) dst[i] = (int16_t)src[i] - 127;
}

/**
 * SSE2: fallback rotate+widen via scalar since SSE2 lacks byte-wise shuffle.
 */
static void DSD_FME_TARGET_ATTR("sse2") widen_rotate90_u8_to_s16_bias127_sse2(const unsigned char *src, int16_t *dst, uint32_t len)
{
    /* Keep scalar logic for correctness without SSSE3 pshufb (SSE2 lacks byte shuffle). */
    widen_rotate90_u8_to_s16_bias127_scalar(src, dst, len);
}
#endif /* x86 */

#if defined(__x86_64__) || defined(__i386__)
/**
 * SSSE3: rotate (1,j,-1,-j) interleaved IQ and widen u8→s16 centered at 127.
 * Tail elements are handled by a scalar helper to preserve the rotation pattern.
 */
static void DSD_FME_TARGET_ATTR("ssse3") widen_rotate90_u8_to_s16_bias127_ssse3(const unsigned char *src, int16_t *dst, uint32_t len)
{
    uint32_t i = 0;
    const __m128i shuffle = _mm_setr_epi8(
        0, 1, 3, 2, 4, 5, 7, 6,  8, 9,11,10,12,13,15,14);
    const __m128i mask_sel = _mm_setr_epi16(
        0x0000,0x0000,0xFFFF,0x0000,0xFFFF,0xFFFF,0x0000,0xFFFF);
    const __m128i c127 = _mm_set1_epi16(127);
    const __m128i zero = _mm_setzero_si128();
    for (; i + 16 <= len; i += 16) {
        __m128i v8 = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i sh = _mm_shuffle_epi8(v8, shuffle);
        __m128i v16_lo = _mm_unpacklo_epi8(sh, zero);
        __m128i v16_hi = _mm_unpackhi_epi8(sh, zero);
        __m128i bs_lo = _mm_sub_epi16(v16_lo, c127);
        __m128i bm_lo = _mm_sub_epi16(c127,  v16_lo);
        __m128i bs_hi = _mm_sub_epi16(v16_hi, c127);
        __m128i bm_hi = _mm_sub_epi16(c127,  v16_hi);
        __m128i out_lo = _mm_or_si128(_mm_and_si128(bm_lo, mask_sel), _mm_andnot_si128(mask_sel, bs_lo));
        __m128i out_hi = _mm_or_si128(_mm_and_si128(bm_hi, mask_sel), _mm_andnot_si128(mask_sel, bs_hi));
        _mm_storeu_si128((__m128i*)(dst + i), out_lo);
        _mm_storeu_si128((__m128i*)(dst + i + 8), out_hi);
    }
    if (i < len) {
        widen_rotate90_u8_to_s16_bias127_scalar(src + i, dst + i, len - i);
    }
}
#endif /* x86 */

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
/**
 * NEON: widen unsigned bytes to signed 16-bit centered at 127.
 */
static void widen_u8_to_s16_bias127_neon(const unsigned char *src, int16_t *dst, uint32_t len)
{
    uint32_t i = 0;
    const uint8x8_t bias8 = vdup_n_u8(127);
    for (; i + 16 <= len; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        uint8x8_t v_lo = vget_low_u8(v);
        uint8x8_t v_hi = vget_high_u8(v);
        int16x8_t lo = vsubl_u8(v_lo, bias8);
        int16x8_t hi = vsubl_u8(v_hi, bias8);
        vst1q_s16(dst + i, lo);
        vst1q_s16(dst + i + 8, hi);
    }
    for (; i < len; i++) dst[i] = (int16_t)src[i] - 127;
}

/**
 * NEON: rotate (1,j,-1,-j) interleaved IQ and widen u8→s16 centered at 127.
 * Uses table lookup on aarch64; on ARMv7 (no vqtbl1q_u8) falls back to scalar.
 */
static void widen_rotate90_u8_to_s16_bias127_neon(const unsigned char *src, int16_t *dst, uint32_t len)
{
#if defined(__aarch64__)
    const uint8x16_t tbl_idx = {0,1,3,2,4,5,7,6, 8,9,11,10,12,13,15,14};
    const int16x8_t c127 = vdupq_n_s16(127);
    const uint16_t mpat[8] = {0x0000,0x0000,0xFFFF,0x0000,0xFFFF,0xFFFF,0x0000,0xFFFF};
    const uint16x8_t msel = vld1q_u16(mpat);
    uint32_t i = 0;
    for (; i + 16 <= len; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        uint8x16_t sh = vqtbl1q_u8(v, tbl_idx);
        uint8x8_t sh_lo8 = vget_low_u8(sh);
        uint8x8_t sh_hi8 = vget_high_u8(sh);
        int16x8_t v16_lo = vreinterpretq_s16_u16(vmovl_u8(sh_lo8));
        int16x8_t v16_hi = vreinterpretq_s16_u16(vmovl_u8(sh_hi8));
        int16x8_t bs_lo = vsubq_s16(v16_lo, c127);
        int16x8_t bm_lo = vsubq_s16(c127,   v16_lo);
        int16x8_t bs_hi = vsubq_s16(v16_hi, c127);
        int16x8_t bm_hi = vsubq_s16(c127,   v16_hi);
        int16x8_t out_lo = vbslq_s16(msel, bm_lo, bs_lo);
        int16x8_t out_hi = vbslq_s16(msel, bm_hi, bs_hi);
        vst1q_s16(dst + i, out_lo);
        vst1q_s16(dst + i + 8, out_hi);
    }
    /* Tail: preserve rotation via scalar helper */
    if (i < len) {
        widen_rotate90_u8_to_s16_bias127_scalar(src + i, dst + i, len - i);
    }
#else
    /* ARMv7 NEON lacks vqtbl1q_u8; use scalar fallback for rotate+widen */
    widen_rotate90_u8_to_s16_bias127_scalar(src, dst, len);
#endif
}
#endif

/**
 * Runtime CPU feature detection and dispatch binding for widening/rotation.
 *
 * Detects available SIMD features (AVX2/SSSE3/SSE2/NEON) and binds the
 * function pointers `g_widen_impl` and `g_widen_rot_impl` accordingly.
 * Safe to call concurrently; guarded by pthread_once in the public wrapper.
 */
static void dsd_fme_init_runtime_dispatch_once(void)
{

    int use_avx2 = 0;
    int use_sse2 = 0;
    int use_ssse3 = 0;

#if defined(__x86_64__) || defined(__i386__)
    unsigned int eax=0, ebx=0, ecx=0, edx=0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        use_sse2 = (edx & bit_SSE2) ? 1 : 0;
        use_ssse3 = (ecx & bit_SSSE3) ? 1 : 0;
        int osxsave = (ecx & bit_OSXSAVE) ? 1 : 0;
        int avx = (ecx & bit_AVX) ? 1 : 0;
        if (osxsave && avx) {
            uint32_t xcr0_lo=0, xcr0_hi=0;
            /* xgetbv ecx=0 is non-privileged on userland */
            __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
            uint64_t xcr0 = ((uint64_t)xcr0_hi << 32) | xcr0_lo;
            if ((xcr0 & 0x6) == 0x6) {
                unsigned int eax7=0, ebx7=0, ecx7=0, edx7=0;
                if (__get_cpuid_count(7, 0, &eax7, &ebx7, &ecx7, &edx7)) {
                    use_avx2 = (ebx7 & bit_AVX2) ? 1 : 0;
                }
            }
        }
    }
#if defined(__x86_64__)
    if (!use_sse2) use_sse2 = 1; /* mandatory on x86_64 */
#endif
#endif

#if defined(__aarch64__)
    int use_neon = 1; /* ASIMD mandatory */
#elif defined(__arm__)
    int use_neon = 0;
#if defined(__linux__)
    unsigned long hw = getauxval(AT_HWCAP);
    /* HWCAP_NEON may be undefined on some headers; fallback to known value 4096 */
#ifndef HWCAP_NEON
#define HWCAP_NEON 4096
#endif
    use_neon = (hw & HWCAP_NEON) ? 1 : 0;
#endif
#endif

    /* Fallbacks */
    g_widen_impl = &widen_u8_to_s16_bias127_scalar;
    g_widen_rot_impl = &widen_rotate90_u8_to_s16_bias127_scalar;

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    if (use_neon) {
        g_widen_impl = &widen_u8_to_s16_bias127_neon;
        g_widen_rot_impl = &widen_rotate90_u8_to_s16_bias127_neon;
        return;
    }
#endif

#if defined(__x86_64__) || defined(__i386__)
    if (use_avx2) {
        g_widen_impl = &widen_u8_to_s16_bias127_avx2;
        g_widen_rot_impl = &widen_rotate90_u8_to_s16_bias127_avx2;
        return;
    }
    if (use_ssse3 || use_sse2) {
        if (use_sse2) g_widen_impl = &widen_u8_to_s16_bias127_sse2;
        if (use_ssse3) g_widen_rot_impl = &widen_rotate90_u8_to_s16_bias127_ssse3;
        else           g_widen_rot_impl = &widen_rotate90_u8_to_s16_bias127_sse2;
        return;
    }
#endif
}

/**
 * Ensure SIMD dispatch is initialized (thread-safe, idempotent).
 */
static void dsd_fme_init_runtime_dispatch(void)
{
	/* Thread-safe, idempotent initialization */
	pthread_once(&dsd_fme_dispatch_once_control, dsd_fme_init_runtime_dispatch_once);
}

/**
 * Saturate 32-bit integer to 16-bit range.
 *
 * @param x Input 32-bit value.
 * @return Clamped 16-bit value in [-32768, 32767].
 */
static inline int16_t sat16(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

/* Runtime flag (default enabled). Set DSD_FME_HB_DECIM=0 to use legacy decimator */
static int use_halfband_decimator = 1;

/* 15-tap half-band low-pass coefficients, Q15 scaled.
   Odd-indexed taps are zero; center tap is 0.5 (16384). The remaining even taps
   sum to 0.5 to yield unity DC gain. Coefficients are symmetric. */
#define HB_TAPS 15
#define HB_HALF ((HB_TAPS - 1) / 2)
static const int16_t hb_q15_taps[HB_TAPS] = {
	-108,    0,  1800,    0,  -500,    0,  7000, 16384,
	 7000,   0,  -500,    0,  1800,    0,   -108
};

/**
 * Decimate one real channel by 2 using a half-band FIR with persistent left history.
 *
 * @param in   Pointer to real input samples.
 * @param in_len Number of real input samples.
 * @param out  Pointer to output buffer (size >= in_len/2).
 * @param hist Persistent history of length HB_TAPS-1 (left wing).
 * @return Number of output samples written (in_len/2).
 */
static inline int hb_decim2_real(const int16_t *in, int in_len, int16_t *out, int16_t *hist)
{
	const int hist_len = HB_TAPS - 1;
	/* Pad right side by repeating last sample to avoid needing future context */
	int16_t last = (in_len > 0) ? in[in_len - 1] : 0;
	/* For simplicity, operate via a small ringless window into a temp view using hist + in + right pad (virtually). */
	int out_len = in_len >> 1; /* floor */
	for (int n = 0; n < out_len; n++) {
		int center_idx = hist_len + (n << 1); /* position in the concatenated [hist | in] domain */
		/* Half-band optimization: only even taps and the center tap contribute (symmetric). */
		const int16_t c0 = hb_q15_taps[0];
		const int16_t c2 = hb_q15_taps[2];
		const int16_t c4 = hb_q15_taps[4];
		const int16_t c6 = hb_q15_taps[6];
		const int16_t c7 = hb_q15_taps[7]; /* center */
		auto get_sample = [&](int src_idx) -> int16_t {
			if (src_idx < hist_len) {
				return hist[src_idx];
			} else {
				int rel = src_idx - hist_len;
				return (rel < in_len) ? in[rel] : last;
			}
		};
		int16_t xc  = get_sample(center_idx);
		int16_t xm1 = get_sample(center_idx - 1);
		int16_t xp1 = get_sample(center_idx + 1);
		int16_t xm3 = get_sample(center_idx - 3);
		int16_t xp3 = get_sample(center_idx + 3);
		int16_t xm5 = get_sample(center_idx - 5);
		int16_t xp5 = get_sample(center_idx + 5);
		int16_t xm7 = get_sample(center_idx - 7);
		int16_t xp7 = get_sample(center_idx + 7);
		int64_t acc = 0;
		acc += (int32_t)c7 * (int32_t)xc;
		acc += (int32_t)c6 * (int32_t)(xm1 + xp1);
		acc += (int32_t)c4 * (int32_t)(xm3 + xp3);
		acc += (int32_t)c2 * (int32_t)(xm5 + xp5);
		acc += (int32_t)c0 * (int32_t)(xm7 + xp7);
		/* Q15 -> Q0 with rounding */
		acc += (1 << 14);
		int32_t y = (int32_t)(acc >> 15);
		out[n] = sat16(y);
	}
	/* Update history with the last (HB_TAPS-1) input samples for next call */
	if (in_len >= hist_len) {
		memcpy(hist, in + (in_len - hist_len), (size_t)hist_len * sizeof(int16_t));
	} else {
		/* Not enough samples: keep tail of previous hist and append current */
		int need = hist_len - in_len;
		if (need > 0) {
			/* shift left existing hist */
			memmove(hist, hist + in_len, (size_t)need * sizeof(int16_t));
		}
		memcpy(hist + need, in, (size_t)in_len * sizeof(int16_t));
	}
	return out_len;
}

/**
 * Fused interleaved complex half-band decimator. Decimates I and Q in one pass
 * without deinterleaving. Exploits the half-band property (zero odd taps) and
 * symmetry; Q15 taps with rounding preserve unity DC gain.
 *
 * @param in      Pointer to interleaved input samples (I,Q,I,Q,...).
 * @param in_len  Number of interleaved int16_t values in input.
 * @param out     Pointer to interleaved output buffer (size >= in_len/2).
 * @param hist_i  Persistent I-channel history of length HB_TAPS-1.
 * @param hist_q  Persistent Q-channel history of length HB_TAPS-1.
 * @return Interleaved output length (in_len/2).
 */
static inline int hb_decim2_complex_interleaved(const int16_t *in, int in_len, int16_t *out,
    int16_t *hist_i, int16_t *hist_q)
{
    const int hist_len = HB_TAPS - 1;
    int ch_len = in_len >> 1; /* per-channel samples */
    int out_ch_len = ch_len >> 1; /* decimated per-channel */
    if (out_ch_len <= 0) {
        return 0;
    }
    int16_t lastI = (ch_len > 0) ? in[in_len - 2] : 0;
    int16_t lastQ = (ch_len > 0) ? in[in_len - 1] : 0;
    for (int n = 0; n < out_ch_len; n++) {
        int center_idx = hist_len + (n << 1); /* per-channel index */
        /* Half-band optimization: only even taps and the center tap contribute (symmetric). */
        const int16_t c0 = hb_q15_taps[0];
        const int16_t c2 = hb_q15_taps[2];
        const int16_t c4 = hb_q15_taps[4];
        const int16_t c6 = hb_q15_taps[6];
        const int16_t c7 = hb_q15_taps[7]; /* center */
        auto get_iq = [&](int src_idx, int16_t &xi, int16_t &xq) {
            if (src_idx < hist_len) {
                xi = hist_i[src_idx];
                xq = hist_q[src_idx];
            } else {
                int rel = src_idx - hist_len;
                if (rel < ch_len) {
                    xi = in[(size_t)(rel << 1)];
                    xq = in[(size_t)(rel << 1) + 1];
                } else {
                    xi = lastI;
                    xq = lastQ;
                }
            }
        };
        int16_t ci, cq;
        int16_t im1, qm1, ip1, qp1;
        int16_t im3, qm3, ip3, qp3;
        int16_t im5, qm5, ip5, qp5;
        int16_t im7, qm7, ip7, qp7;
        get_iq(center_idx, ci, cq);
        get_iq(center_idx - 1, im1, qm1);
        get_iq(center_idx + 1, ip1, qp1);
        get_iq(center_idx - 3, im3, qm3);
        get_iq(center_idx + 3, ip3, qp3);
        get_iq(center_idx - 5, im5, qm5);
        get_iq(center_idx + 5, ip5, qp5);
        get_iq(center_idx - 7, im7, qm7);
        get_iq(center_idx + 7, ip7, qp7);
        int64_t accI = 0;
        int64_t accQ = 0;
        accI += (int32_t)c7 * (int32_t)ci;
        accQ += (int32_t)c7 * (int32_t)cq;
        accI += (int32_t)c6 * (int32_t)(im1 + ip1);
        accQ += (int32_t)c6 * (int32_t)(qm1 + qp1);
        accI += (int32_t)c4 * (int32_t)(im3 + ip3);
        accQ += (int32_t)c4 * (int32_t)(qm3 + qp3);
        accI += (int32_t)c2 * (int32_t)(im5 + ip5);
        accQ += (int32_t)c2 * (int32_t)(qm5 + qp5);
        accI += (int32_t)c0 * (int32_t)(im7 + ip7);
        accQ += (int32_t)c0 * (int32_t)(qm7 + qp7);
        accI += (1 << 14);
        accQ += (1 << 14);
        int32_t yI = (int32_t)(accI >> 15);
        int32_t yQ = (int32_t)(accQ >> 15);
        out[(size_t)(n << 1)]     = sat16(yI);
        out[(size_t)(n << 1) + 1] = sat16(yQ);
    }
    /* Update histories with last HB_TAPS-1 per-channel input samples */
    if (ch_len >= hist_len) {
        int start = ch_len - hist_len;
        for (int k = 0; k < hist_len; k++) {
            int rel = start + k;
            hist_i[k] = in[(size_t)(rel << 1)];
            hist_q[k] = in[(size_t)(rel << 1) + 1];
        }
    } else {
        int need = hist_len - ch_len;
        if (need > 0) {
            memmove(hist_i, hist_i + ch_len, (size_t)need * sizeof(int16_t));
            memmove(hist_q, hist_q + ch_len, (size_t)need * sizeof(int16_t));
        }
        for (int k = 0; k < ch_len; k++) {
            hist_i[need + k] = in[(size_t)(k << 1)];
            hist_q[need + k] = in[(size_t)(k << 1) + 1];
        }
    }
    return out_ch_len << 1; /* interleaved length */
}

static void atan_lut_once_init(void)
{
	int i;
	atan_lut = static_cast<int*>(malloc(atan_lut_size * sizeof(int)));
	if (atan_lut == NULL) {
		return;
	}
	for (i = 0; i < atan_lut_size; i++) {
		atan_lut[i] = (int) (atan((double) i / (1<<atan_lut_coef)) / kPi * (1<<14));
	}
}

/* Build quarter-wave sine LUT in Q15: sin(theta) where theta in [0, pi/2] */
static void fll_lut_once_init(void)
{
	for (int i = 0; i <= 1024; i++) {
		double theta = (kPi * 0.5) * ((double)i / 1024.0);
		int v = (int)lrint(sin(theta) * 32767.0);
		if (v >  32767) v =  32767;
		if (v < -32767) v = -32767;
		fll_qsine_q15_lut[i] = (int16_t)v;
}
}

/**
 * Compute sin/cos in Q15 from phase using a quarter-wave sine LUT.
 * The choice to use the LUT vs. a fast piecewise approximation is
 * made by the caller (see fll_mix_and_update).
 *
 * @param phase_q15 Phase accumulator (Q15, wrap at 2*pi -> 1<<15 scale).
 * @param c_out     [out] Cosine Q15.
 * @param s_out     [out] Sine Q15.
 */
static inline void fll_sin_cos_q15_from_phase_lut(int phase_q15, int16_t *c_out, int16_t *s_out)
{
	/* phase_q15 wraps at 1<<15 mapping to 2*pi */
	int p = phase_q15 & 0x7FFF; /* 0..32767 */
	int quad = p >> 13;         /* 0..3 */
	int r = p & 0x1FFF;         /* position within quadrant: 0..8191 */

	/* Helper to sample quarter-wave S(r) with r in [0..8192] using 1024-segment linear interp */
	auto sample_quarter = [](int r8192) -> int16_t {
        if (r8192 < 0) r8192 = 0;
        if (r8192 > 8192) r8192 = 8192;
        int idx = r8192 >> 3;          /* 0..1024 */
        int frac = r8192 & 7;          /* 0..7 */
        int16_t s0 = fll_qsine_q15_lut[idx];
        int16_t s1 = fll_qsine_q15_lut[(idx < 1024) ? (idx + 1) : 1024];
        int diff = (int)s1 - (int)s0;
        int interp = (int)s0 + ((diff * frac + 4) >> 3); /* rounded */
        if (interp >  32767) interp =  32767;
        if (interp < -32767) interp = -32767;
        return (int16_t)interp;
    };

	int16_t s_pos, c_pos;
	/* Cosine within quadrant uses complementary angle in the quarter-wave */
    switch (quad) {
    case 0: /* [0, pi/2) */
        s_pos = sample_quarter(r);
        c_pos = sample_quarter(8192 - r);
        *s_out = s_pos;
        *c_out = c_pos;
        break;
    case 1: /* [pi/2, pi) */
        s_pos = sample_quarter(8192 - r);
        c_pos = sample_quarter(r);
        *s_out = s_pos;
        *c_out = (int16_t)(-c_pos);
        break;
    case 2: /* [pi, 3pi/2) */
        s_pos = sample_quarter(r);
        c_pos = sample_quarter(8192 - r);
        *s_out = (int16_t)(-s_pos);
        *c_out = (int16_t)(-c_pos);
        break;
    default: /* 3: [3pi/2, 2pi) */
        s_pos = sample_quarter(8192 - r);
        c_pos = sample_quarter(r);
        *s_out = (int16_t)(-s_pos);
        *c_out = c_pos;
        break;
    }
}

//UDP -- keep for compatibility reasons
#include <netinet/in.h>
#include <arpa/inet.h>
static pthread_t socket_freq;

int rtl_bandwidth;
int bandwidth_multiplier;
int bandwidth_divisor = 48000; //divide bandwidth by this to get multiplier for the for j loop to queue.push

short int volume_multiplier;
short int port;
struct dongle_state
{
	int      exit_flag;
	pthread_t thread;
	rtlsdr_dev_t *dev;
	int      dev_index;
	uint32_t freq;
	uint32_t rate;
	int      gain;
	uint32_t buf_len;
	int      ppm_error;
	int      offset_tuning;
	int      direct_sampling;
	std::atomic<int> mute;
	struct demod_state *demod_target;
};

struct demod_state
{
	int      exit_flag;
	pthread_t thread;
	int16_t  *lowpassed;
	/* Scratch buffer for demod thread to read blocks from the input ring */
	/* Not a ring; callback writes directly into the global input ring. */
	alignas(DSD_FME_ALIGN) int16_t  input_cb_buf[MAXIMUM_BUF_LENGTH];
	int      lp_len;
	int16_t  lp_i_hist[10][6];
	int16_t  lp_q_hist[10][6];
	alignas(DSD_FME_ALIGN) int16_t  result[MAXIMUM_BUF_LENGTH];
	int16_t  droop_i_hist[9];
	int16_t  droop_q_hist[9];
	int      result_len;
	int      rate_in;
	int      rate_out;
	int      rate_out2;
	int      now_r, now_j;
	int      pre_r, pre_j;
	int      prev_index;
	int      downsample;    /* min 1, max 256 */
	int      post_downsample;
	int      output_scale;
	int      squelch_level, conseq_squelch, squelch_hits, terminate_on_squelch;
	/* Incremental, decimated RMS squelch estimator (power-domain, sqrt-free) */
	int64_t  squelch_running_power;
	int      squelch_decim_stride;
	int      squelch_decim_phase;
	int      squelch_window;
	int      downsample_passes;
	int      comp_fir_size;
	int      custom_atan;
	int      deemph, deemph_a;
	int      deemph_avg;
	/* Optional post-demod audio low-pass filter (one-pole) */
	int      audio_lpf_enable;
	int      audio_lpf_alpha;   /* Q15 alpha for one-pole LPF */
	int      audio_lpf_state;   /* state/output y[n-1] in Q0 */
	int      now_lpr;
	int      prev_lpr_index;
	int      dc_block, dc_avg;
	/* Half-band decimator state */
	int16_t  hb_workbuf[MAXIMUM_BUF_LENGTH];
	int16_t  hb_hist_i[10][HB_TAPS-1];
	int16_t  hb_hist_q[10][HB_TAPS-1];
	/* Reserved buffers for potential deinterleave path (currently unused) */
	alignas(DSD_FME_ALIGN) int16_t  hb_i_buf[MAXIMUM_BUF_LENGTH/2];
	alignas(DSD_FME_ALIGN) int16_t  hb_q_buf[MAXIMUM_BUF_LENGTH/2];
	alignas(DSD_FME_ALIGN) int16_t  hb_i_out[MAXIMUM_BUF_LENGTH/2];
	alignas(DSD_FME_ALIGN) int16_t  hb_q_out[MAXIMUM_BUF_LENGTH/2];
	/* Preallocated buffer for linear upsampler (bandwidth_multiplier) */
	alignas(DSD_FME_ALIGN) int16_t  upsample_buf[MAXIMUM_BUF_LENGTH * MAX_BANDWIDTH_MULTIPLIER];
	/* Polyphase rational resampler (L/M) state and output buffer */
	int      resamp_enabled;
	int      resamp_target_hz;     /* desired output sample rate */
	int      resamp_L;             /* upsample factor */
	int      resamp_M;             /* downsample factor */
	int      resamp_phase;         /* 0..L-1 accumulator */
	int      resamp_taps_len;      /* prototype taps length (padded to K*L) */
	int      resamp_taps_per_phase;/* K = ceil(taps_len/L) */
	int16_t *resamp_taps;          /* Q15 taps, length = K*L */
	int16_t *resamp_hist;          /* circular history, length = K */
	int      resamp_hist_head;     /* head index into circular history [0..K-1] */
	/* Output buffer for resampler (worst-case 4x expansion) */
	alignas(DSD_FME_ALIGN) int16_t  resamp_outbuf[MAXIMUM_BUF_LENGTH * 4];
	/* Residual CFO loop (FLL) state */
	int      fll_enabled;
	int      fll_alpha_q15;   /* proportional gain (Q15) */
	int      fll_beta_q15;    /* integral gain (Q15) */
	int      fll_freq_q15;    /* NCO frequency increment (Q15 radians/sample scaled) */
	int      fll_phase_q15;   /* NCO phase accumulator (wrap at 2*pi -> 1<<15 scale) */
	int      fll_prev_r;
	int      fll_prev_j;
	/* Timing error detector (Gardner) fractional-delay state */
	int      ted_enabled;
	int      ted_force;       /* allow forcing TED even for FM/C4FM paths */
	int      ted_gain_q20;    /* small gain (Q20) for stability */
	int      ted_sps;         /* nominal samples per symbol (e.g., 10 for 4800 sym/s at 48k) */
	int      ted_mu_q20;      /* fractional phase [0,1) in Q20 */
	/* Work buffer for timing-adjusted I/Q */
	alignas(DSD_FME_ALIGN) int16_t  timing_buf[MAXIMUM_BUF_LENGTH];
	/* Minimal 2-thread worker pool for intra-block parallelism */
	int      mt_enabled;
	int      mt_ready;
	pthread_t mt_threads[2];
	pthread_mutex_t mt_lock;
	pthread_cond_t  mt_cv;
	pthread_cond_t  mt_done_cv;
	int      mt_should_exit;
	int      mt_epoch;
	int      mt_completed_in_epoch;
	int      mt_posted_count;
	struct { void (*run)(void*); void *arg; } mt_tasks[2];
	int      mt_worker_id[2];
	struct { struct demod_state *s; int id; } mt_args[2];
	int      (*discriminator)(int, int, int, int);
	void     (*mode_demod)(struct demod_state*);
	/* Ready/condvar kept for cleanup compatibility; input ring is a global SPSC ring */
	pthread_cond_t ready; /* kept for cleanup compatibility; unused now */
	pthread_mutex_t ready_m;
	struct output_state *output_target;
};

/* Forward declarations for minimal worker pool helpers */
static void demod_mt_init(struct demod_state *s);
static void demod_mt_destroy(struct demod_state *s);
static void demod_mt_run_two(struct demod_state *s, void (*f0)(void*), void *a0, void (*f1)(void*), void *a1);


struct demod_mt_worker_arg { struct demod_state *s; int id; };

/**
 * Worker thread procedure for the minimal 2-thread DEMOD pool.
 * Waits for posted tasks, executes them, and signals completion.
 *
 * @param arg Pointer to `demod_mt_worker_arg` with owning state and worker id.
 * @return NULL when the worker exits.
 */
static void *demod_mt_worker(void *arg)
{
	struct demod_mt_worker_arg *wa = (struct demod_mt_worker_arg*)arg;
	struct demod_state *s = wa->s;
	const int id = wa->id;
	int local_epoch = 0;
	for (;;) {
		pthread_mutex_lock(&s->mt_lock);
		while (!s->mt_should_exit && s->mt_epoch == local_epoch) {
			pthread_cond_wait(&s->mt_cv, &s->mt_lock);
		}
		if (s->mt_should_exit) {
			pthread_mutex_unlock(&s->mt_lock);
			break;
		}
		local_epoch = s->mt_epoch;
		void (*fn)(void*) = NULL;
		void *fn_arg = NULL;
		if (id < s->mt_posted_count) {
			fn = s->mt_tasks[id].run;
			fn_arg = s->mt_tasks[id].arg;
		}
		pthread_mutex_unlock(&s->mt_lock);
		if (fn) {
			fn(fn_arg);
		}
		pthread_mutex_lock(&s->mt_lock);
		s->mt_completed_in_epoch++;
		if (s->mt_completed_in_epoch >= s->mt_posted_count) {
			pthread_cond_signal(&s->mt_done_cv);
		}
		pthread_mutex_unlock(&s->mt_lock);
	}
	return NULL;
}

/**
 * Initialize the minimal 2-thread worker pool for intra-block tasks.
 * Enabled when `DSD_FME_MT=1` in the environment.
 *
 * @param s Demodulator state to initialize with worker threads.
 */
static void demod_mt_init(struct demod_state *s)
{
	const char *mt = getenv("DSD_FME_MT");
	s->mt_enabled = (mt && mt[0] == '1') ? 1 : 0;
	s->mt_should_exit = 0;
	s->mt_epoch = 0;
	s->mt_completed_in_epoch = 0;
	s->mt_posted_count = 0;
	if (!s->mt_enabled) {
		return;
	}
	pthread_mutex_init(&s->mt_lock, NULL);
	pthread_cond_init(&s->mt_cv, NULL);
	pthread_cond_init(&s->mt_done_cv, NULL);
	/* Start two workers */
	for (int i = 0; i < 2; i++) {
		s->mt_args[i].s = s;
		s->mt_args[i].id = i;
		pthread_create(&s->mt_threads[i], NULL, demod_mt_worker, (void*)&s->mt_args[i]);
	}
	fprintf(stderr, "Intra-block multithreading enabled (DSD_FME_MT=1), workers: 2.\n");
}

/**
 * Tear down the minimal worker pool created by demod_mt_init.
 *
 * @param s Demodulator state whose worker pool will be destroyed.
 */
static void demod_mt_destroy(struct demod_state *s)
{
	if (!s->mt_enabled) return;
	pthread_mutex_lock(&s->mt_lock);
	s->mt_should_exit = 1;
	pthread_cond_broadcast(&s->mt_cv);
	pthread_mutex_unlock(&s->mt_lock);
	for (int i = 0; i < 2; i++) {
		pthread_join(s->mt_threads[i], NULL);
	}
	pthread_cond_destroy(&s->mt_done_cv);
	pthread_cond_destroy(&s->mt_cv);
	pthread_mutex_destroy(&s->mt_lock);
}

/**
 * Post up to two tasks to the worker pool and wait for their completion.
 * If the pool is disabled, runs tasks synchronously on the caller thread.
 *
 * @param s  Demodulator state with worker pool.
 * @param f0 Task 0 function pointer (may be NULL).
 * @param a0 Task 0 argument.
 * @param f1 Task 1 function pointer (may be NULL).
 * @param a1 Task 1 argument.
 */
static void demod_mt_run_two(struct demod_state *s, void (*f0)(void*), void *a0, void (*f1)(void*), void *a1)
{
	if (!s->mt_enabled) {
		if (f0) f0(a0);
		if (f1) f1(a1);
		return;
	}
	pthread_mutex_lock(&s->mt_lock);
	s->mt_tasks[0].run = f0; s->mt_tasks[0].arg = a0;
	s->mt_tasks[1].run = f1; s->mt_tasks[1].arg = a1;
	s->mt_posted_count = (f1 != NULL) ? 2 : 1;
	s->mt_completed_in_epoch = 0;
	s->mt_epoch++;
	pthread_cond_broadcast(&s->mt_cv);
	while (s->mt_completed_in_epoch < s->mt_posted_count) {
		pthread_cond_wait(&s->mt_done_cv, &s->mt_lock);
	}
	pthread_mutex_unlock(&s->mt_lock);
}

struct output_state
{
	int      rate;
	int16_t  *buffer;
	size_t   capacity;
	std::atomic<size_t> head;
	std::atomic<size_t> tail;
	pthread_cond_t ready;
	pthread_cond_t space;
	pthread_mutex_t ready_m;
};

/* Simple SPSC ring for interleaved I/Q int16_t samples (input path) */
struct input_ring_state
{
	int16_t  *buffer;
	size_t   capacity;   /* in int16_t elements */
	std::atomic<size_t> head;
	std::atomic<size_t> tail;
	pthread_cond_t ready;
	pthread_mutex_t ready_m;
};

/**
 * Number of samples currently in the input ring.
 */
static inline size_t input_ring_used(const struct input_ring_state *r)
{
	size_t h = r->head.load();
	size_t t = r->tail.load();
	if (h >= t) return h - t;
	return r->capacity - (t - h);
}

/**
 * Number of free slots available for writing in the input ring.
 */
static inline size_t input_ring_free(const struct input_ring_state *r)
{
	return (r->capacity - 1) - input_ring_used(r);
}

/**
 * Check if the input ring is empty.
 */
static inline int input_ring_is_empty(const struct input_ring_state *r)
{
	return r->head.load() == r->tail.load();
}

/**
 * Clear the input ring head/tail indices.
 */
static inline void input_ring_clear(struct input_ring_state *r)
{
	r->tail.store(0);
	r->head.store(0);
}

/**
 * Reserve up to two contiguous writable regions totaling at least min_needed
 * (or less if near full).
 *
 * May drop oldest half when full to avoid blocking.
 *
 * @param r          Input ring buffer state.
 * @param min_needed Minimum samples requested for writing.
 * @param p1         [out] First writable region pointer or NULL.
 * @param n1         [out] First writable region length.
 * @param p2         [out] Second writable region pointer or NULL.
 * @param n2         [out] Second writable region length.
 * @return Total writable samples granted across regions.
 */
static int input_ring_reserve(struct input_ring_state *r, size_t min_needed,
                       int16_t **p1, size_t *n1, int16_t **p2, size_t *n2)
{
	size_t free_sp = input_ring_free(r);
	/* Producer must never advance consumer tail; if full, grant nothing */
	/* Provide up to min(free_sp, min_needed) across at most two regions */
	size_t grant = (min_needed < free_sp) ? min_needed : free_sp;
	size_t h = r->head.load();
	size_t first = r->capacity - h;
	if (first > grant) first = grant;
	*p1 = (first > 0) ? (r->buffer + h) : NULL;
	*n1 = first;
	*p2 = NULL;
	*n2 = 0;
	if (grant > first) {
		*p2 = r->buffer;
		*n2 = grant - first;
	}
	return (int)(*n1 + *n2);
}

/**
 * Commit produced samples and signal consumer on empty→non-empty transition.
 *
 * @param r         Input ring buffer state.
 * @param produced  Number of samples produced to commit.
 */
static void input_ring_commit(struct input_ring_state *r, size_t produced)
{
	if (produced == 0) return;
	int need_signal = input_ring_is_empty(r);
	size_t h = r->head.load();
	h += produced;
	if (h >= r->capacity) h %= r->capacity;
	r->head.store(h);
	if (need_signal) {
		pthread_mutex_lock(&r->ready_m);
		pthread_cond_signal(&r->ready);
		pthread_mutex_unlock(&r->ready_m);
	}
}

/**
 * Write up to count samples into the input ring buffer, dropping oldest half
 * when necessary to avoid blocking.
 *
 * @param r     Input ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
static void input_ring_write(struct input_ring_state *r, const int16_t *data, size_t count)
{
	int need_signal = input_ring_is_empty(r);
	while (count > 0 && !exitflag) {
		size_t free_sp = input_ring_free(r);
		if (free_sp == 0) {
			/* Ring full: to avoid racing the consumer, drop remainder */
			break;
		}
		size_t write_now = (count < free_sp) ? count : free_sp;
		size_t h = r->head.load();
		size_t first = r->capacity - h;
		if (first > write_now) first = write_now;
		memcpy(r->buffer + h, data, first * sizeof(int16_t));
		if (write_now > first) {
			memcpy(r->buffer, data + first, (write_now - first) * sizeof(int16_t));
			h = write_now - first;
		} else {
			h += first;
			if (h == r->capacity) h = 0;
		}
		r->head.store(h);
		data += write_now;
		count -= write_now;
	}
	if (need_signal) {
		pthread_mutex_lock(&r->ready_m);
		pthread_cond_signal(&r->ready);
		pthread_mutex_unlock(&r->ready_m);
	}
}

/**
 * Read up to max_count samples from input ring buffer, blocking with timeout
 * until data is available or exit is requested.
 *
 * @param r         Input ring buffer state.
 * @param out       Destination buffer for samples.
 * @param max_count Maximum number of samples to read.
 * @return Number of samples read (>=1), 0 if max_count is 0, or -1 on exit.
 */
static int input_ring_read_block(struct input_ring_state *r, int16_t *out, size_t max_count)
{
	if (max_count == 0) return 0;
	while (input_ring_is_empty(r)) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_nsec += 10L * 1000000L; /* 10ms */
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_sec += ts.tv_nsec / 1000000000L;
			ts.tv_nsec = ts.tv_nsec % 1000000000L;
		}
		pthread_mutex_lock(&r->ready_m);
		pthread_cond_timedwait(&r->ready, &r->ready_m, &ts);
		pthread_mutex_unlock(&r->ready_m);
		if (exitflag) return -1;
	}
	size_t available = input_ring_used(r);
	size_t read_now = (max_count < available) ? max_count : available;
	size_t t = r->tail.load();
	size_t first = r->capacity - t;
	if (first > read_now) first = read_now;
	memcpy(out, r->buffer + t, first * sizeof(int16_t));
	t += first;
	if (t == r->capacity) t = 0;
	if (read_now > first) {
		memcpy(out + first, r->buffer, (read_now - first) * sizeof(int16_t));
		t = read_now - first;
	}
	r->tail.store(t);
	return (int)read_now;
}
struct controller_state
{
	int      exit_flag;
	pthread_t thread;
	uint32_t freqs[FREQUENCIES_LIMIT];
	int      freq_len;
	int      freq_now;
	int      edge;
	int      wb_mode;
	pthread_cond_t hop;
	pthread_mutex_t hop_m;
};

struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;
static struct input_ring_state input_ring;

#define safe_cond_signal(n, m) pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m)
#define safe_cond_wait(n, m) pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m)

/**
 * Optionally enable realtime scheduling and set CPU affinity for the current
 * thread based on environment variables.
 *
 * When `DSD_FME_RT_SCHED=1`, attempts to switch the calling thread to
 * SCHED_FIFO with a priority derived from `DSD_FME_RT_PRIO_<ROLE>` if present.
 * If `DSD_FME_CPU_<ROLE>` is set to a valid CPU index, pins the thread to that
 * CPU.
 *
 * @param role Optional role label (e.g. "DEMOD", "DONGLE") used to look up
 *             per-role environment variables.
 */
static void maybe_set_thread_realtime_and_affinity(const char *role)
{
    const char *enable = getenv("DSD_FME_RT_SCHED");
    if (!enable || enable[0] != '1') {
        return;
    }

    /* Optional: role-specific priority (1..99) for SCHED_FIFO */
    int policy = SCHED_FIFO;
    struct sched_param sp;
    int pmax = sched_get_priority_max(policy);
    int pmin = sched_get_priority_min(policy);
    int def = (pmax > 10) ? (pmax - 10) : pmax; /* default near top, but safe */
    char envname[64];

    sp.sched_priority = def;
    if (role) {
        /* e.g., DSD_FME_RT_PRIO_DEMOD, DSD_FME_RT_PRIO_DONGLE */
        snprintf(envname, sizeof(envname), "DSD_FME_RT_PRIO_%s", role);
        const char *prio_str = getenv(envname);
        if (prio_str && prio_str[0] != '\0') {
            int pr = atoi(prio_str);
            if (pr < pmin) pr = pmin;
            if (pr > pmax) pr = pmax;
            sp.sched_priority = pr;
        }
    }

    if (pthread_setschedparam(pthread_self(), policy, &sp) != 0) {
        fprintf(stderr, "WARNING: Failed to set %s thread to SCHED_FIFO (needs CAP_SYS_NICE).\n", role ? role : "RT");
    } else {
        fprintf(stderr, "%s thread SCHED_FIFO priority set to %d.\n", role ? role : "RT", sp.sched_priority);
    }

    /* Optional: role-specific CPU affinity: DSD_FME_CPU_DEMOD / DSD_FME_CPU_DONGLE */
    if (role) {
        snprintf(envname, sizeof(envname), "DSD_FME_CPU_%s", role);
        const char *cpu_str = getenv(envname);
        if (cpu_str && cpu_str[0] != '\0') {
            int cpu = atoi(cpu_str);
            if (cpu >= 0) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET((unsigned)cpu, &cpuset);
                if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
                    fprintf(stderr, "WARNING: Failed to set CPU affinity for %s thread to CPU %d.\n", role, cpu);
                } else {
                    fprintf(stderr, "%s thread pinned to CPU %d.\n", role, cpu);
                }
            }
        }
    }
}

/**
 * Number of samples currently in the output ring.
 *
 * @param o Output ring state.
 * @return Number of queued samples.
 */
static inline size_t ring_used(const struct output_state *o)
{
    /* Atomics policy: head/tail are atomics. We use default sequential
       consistency for simplicity. In an SPSC ring, this could be relaxed
       to acquire/release without changing behavior. */
    size_t h = o->head.load();
    size_t t = o->tail.load();
    if (h >= t) return h - t;
    return o->capacity - (t - h);
}

/**
 * Number of free slots available in the output ring.
 *
 * @param o Output ring state.
 * @return Number of writable samples before the ring becomes full.
 */
static inline size_t ring_free(const struct output_state *o)
{
    return (o->capacity - 1) - ring_used(o);
}

/**
 * Check if the output ring is empty.
 *
 * @param o Output ring state.
 * @return Non-zero if empty, zero otherwise.
 */
static inline int ring_is_empty(const struct output_state *o)
{
    /* See atomics note in ring_used() for ordering considerations. */
    return o->head.load() == o->tail.load();
}

/**
 * Clear the output ring head/tail indices.
 *
 * @param o Output ring state to clear.
 */
static inline void ring_clear(struct output_state *o)
{
    /* Clearing indices; with relaxed ordering this would be a release store. */
    o->tail.store(0);
    o->head.store(0);
}

/* Write up to count samples, blocking until space is available. Signals data availability after writes. */
static void ring_write(struct output_state *o, const int16_t *data, size_t count)
{
    int need_signal = ring_is_empty(o);
    while (count > 0 && !exitflag) {
        size_t free_sp = ring_free(o);
        if (free_sp == 0) {
            /* Wait for space with timeout to avoid indefinite blocking */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 50L * 1000000L; /* 50ms */
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += ts.tv_nsec / 1000000000L;
                ts.tv_nsec = ts.tv_nsec % 1000000000L;
            }
            pthread_mutex_lock(&o->ready_m);
            pthread_cond_timedwait(&o->space, &o->ready_m, &ts);
            pthread_mutex_unlock(&o->ready_m);
            if (exitflag) break;
            continue;
        }
        size_t write_now = (count < free_sp) ? count : free_sp;
        size_t h = o->head.load();
        size_t first = o->capacity - h;
        if (first > write_now) first = write_now;
        memcpy(o->buffer + h, data, first * sizeof(int16_t));
        if (write_now > first) {
            memcpy(o->buffer, data + first, (write_now - first) * sizeof(int16_t));
            h = write_now - first;
        } else {
            h += first;
            if (h == o->capacity) h = 0;
        }
        o->head.store(h);
        data += write_now;
        count -= write_now;
    }
    if (need_signal) {
        safe_cond_signal(&o->ready, &o->ready_m);
    }
}

/* Same as ring_write but does not signal; caller decides when to signal */
static void ring_write_no_signal(struct output_state *o, const int16_t *data, size_t count)
{
    while (count > 0 && !exitflag) {
        size_t free_sp = ring_free(o);
        if (free_sp == 0) {
            /* Wait for space with timeout to avoid indefinite blocking */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 50L * 1000000L; /* 50ms */
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += ts.tv_nsec / 1000000000L;
                ts.tv_nsec = ts.tv_nsec % 1000000000L;
            }
            pthread_mutex_lock(&o->ready_m);
            pthread_cond_timedwait(&o->space, &o->ready_m, &ts);
            pthread_mutex_unlock(&o->ready_m);
            if (exitflag) break;
            continue;
        }
        size_t write_now = (count < free_sp) ? count : free_sp;
        size_t h = o->head.load();
        size_t first = o->capacity - h;
        if (first > write_now) first = write_now;
        memcpy(o->buffer + h, data, first * sizeof(int16_t));
        if (write_now > first) {
            memcpy(o->buffer, data + first, (write_now - first) * sizeof(int16_t));
            h = write_now - first;
        } else {
            h += first;
            if (h == o->capacity) h = 0;
        }
        o->head.store(h);
        data += write_now;
        count -= write_now;
    }
}

/**
 * Write and signal consumer only if the ring transitions from empty to
 * non-empty. Reduces unnecessary wakeups while ensuring timely reads.
 *
 * @param o     Output ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
static void ring_write_signal_on_empty_transition(struct output_state *o, const int16_t *data, size_t count)
{
    int need_signal = ring_is_empty(o);
    ring_write_no_signal(o, data, count);
    if (need_signal) {
        safe_cond_signal(&o->ready, &o->ready_m);
    }
}

/**
 * Read one sample from the output ring, blocking with timeout until available
 * or exit requested.
 *
 * @param o    Output ring buffer state.
 * @param out  Destination for one sample.
 * @return 0 on success, -1 on exit.
 */
static int ring_read_one(struct output_state *o, int16_t *out)
{
    while (ring_is_empty(o)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 10L * 1000000L; /* 10ms */
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec = ts.tv_nsec % 1000000000L;
        }
        pthread_mutex_lock(&o->ready_m);
        pthread_cond_timedwait(&o->ready, &o->ready_m, &ts);
        pthread_mutex_unlock(&o->ready_m);
        if (exitflag) return -1;
    }
    size_t t = o->tail.load();
    *out = o->buffer[t];
    t += 1;
    if (t == o->capacity) t = 0;
    o->tail.store(t);
    /* Signal space available for producer */
    safe_cond_signal(&o->space, &o->ready_m);
    return 0;
}

/**
 * Read up to max_count samples into out. Blocks until at least one sample is available or exit.
 * Signals producer space once after the batch.
 *
 * @param o         Output ring buffer state.
 * @param out       Destination buffer for samples.
 * @param max_count Maximum number of samples to read.
 * @return Number of samples read (>=1) or -1 on exit.
 */
static int ring_read_batch(struct output_state *o, int16_t *out, size_t max_count)
{
    if (max_count == 0) return 0;
    while (ring_is_empty(o)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 10L * 1000000L; /* 10ms */
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec = ts.tv_nsec % 1000000000L;
        }
        pthread_mutex_lock(&o->ready_m);
        pthread_cond_timedwait(&o->ready, &o->ready_m, &ts);
        pthread_mutex_unlock(&o->ready_m);
        if (exitflag) return -1;
    }

    size_t available = ring_used(o);
    size_t read_now = (max_count < available) ? max_count : available;
    size_t t = o->tail.load();
    size_t first = o->capacity - t;
    if (first > read_now) first = read_now;
    memcpy(out, o->buffer + t, first * sizeof(int16_t));
    t += first;
    if (t == o->capacity) t = 0;
    if (read_now > first) {
        memcpy(out + first, o->buffer, (read_now - first) * sizeof(int16_t));
        t = read_now - first;
    }
    o->tail.store(t);
    /* Signal space available once for the whole batch */
    safe_cond_signal(&o->space, &o->ready_m);
    return (int)read_now;
}

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
	{0,},
	{9, -156,  -97, 2798, -15489, 61019, -15489, 2798,  -97, -156},
	{9, -128, -568, 5593, -24125, 74126, -24125, 5593, -568, -128},
	{9, -129, -639, 6187, -26281, 77511, -26281, 6187, -639, -129},
	{9, -122, -612, 6082, -26353, 77818, -26353, 6082, -612, -122},
	{9, -120, -602, 6015, -26269, 77757, -26269, 6015, -602, -120},
	{9, -120, -582, 5951, -26128, 77542, -26128, 5951, -582, -120},
	{9, -119, -580, 5931, -26094, 77505, -26094, 5931, -580, -119},
	{9, -119, -578, 5921, -26077, 77484, -26077, 5921, -578, -119},
	{9, -119, -577, 5917, -26067, 77473, -26067, 5917, -577, -119},
	{9, -199, -362, 5303, -25505, 77489, -25505, 5303, -362, -199},
};

/**
 * Rotate interleaved IQ bytes by 90 degrees in-place.
 *
 * 90° rotation sequence: 1+0j, 0+1j, -1+0j, 0-1j. Operates on u8
 * interleaved I/Q in-place. Negation is performed as (255 - x) to
 * approximate centered negation for subsequent widening by subtracting 128.
 * Assumes `len` is a multiple of 8 (four I/Q pairs per loop).
 *
 * @param buf Interleaved IQ byte buffer.
 * @param len Buffer length in bytes (processed in blocks of 8).
 */
void rotate_90(unsigned char *buf, uint32_t len)
{
	uint32_t i;
	unsigned char tmp;
	/* Process only full 8-byte blocks (4 IQ pairs) to avoid overrun */
	uint32_t full = len - (len % 8);
	for (i=0; i<full; i+=8) {
		/* uint8_t negation = 255 - x */
		tmp = 255 - buf[i+3];
		buf[i+3] = buf[i+2];
		buf[i+2] = tmp;

		buf[i+4] = 255 - buf[i+4];
		buf[i+5] = 255 - buf[i+5];

		tmp = 255 - buf[i+6];
		buf[i+6] = buf[i+7];
		buf[i+7] = tmp;
	}
}

/**
 * Simple boxcar low-pass accumulator with decimation on interleaved I/Q.
 * Accumulates I and Q independently over `downsample` input samples and
 * writes a single output (I,Q) pair per window. Scaling/normalization is
 * deferred; this function sums and decimates with saturation on writeback.
 *
 * @param d Demodulator state (uses lowpassed buffer and decimation state).
 */
void low_pass(struct demod_state *d)
{
	int i=0, i2=0;
	while (i < d->lp_len) {
		d->now_r += d->lowpassed[i];
		d->now_j += d->lowpassed[i+1];
		i += 2;
		d->prev_index++;
		if (d->prev_index < d->downsample) {
			continue;
		}
		/* Saturate accumulated sums when writing back to int16 */
		d->lowpassed[i2]   = sat16(d->now_r);
		d->lowpassed[i2+1] = sat16(d->now_j);
		d->prev_index = 0;
		d->now_r = 0;
		d->now_j = 0;
		i2 += 2;
	}
	d->lp_len = i2;
}

/**
 * Boxcar low-pass and decimate by step (no wraparound).
 * Length must be a multiple of step.
 *
 * @param signal2 In/out buffer of samples.
 * @param len     Length of input buffer.
 * @param step    Decimation factor.
 * @return New length after decimation.
 */
int low_pass_simple(int16_t *signal2, int len, int step)
{
	int i, i2, sum;
	if (step <= 0) return len;
	for(i=0; i + (step-1) < len; i+=step) {
		sum = 0;
		for(i2=0; i2<step; i2++) {
			sum += (int)signal2[i + i2];
		}
		// Normalize by step with rounding. Writes output at i/step index.
		int val = (sum >= 0) ? (sum + step/2) / step : -(((-sum) + step/2) / step);
		signal2[i/step] = (int16_t)val;
	}
	/* Duplicate the final sample to provide one-sample lookahead for callers
	   that expect at least one extra element. */
	int out_len = len / step;
	if (out_len > 0) {
		signal2[out_len] = signal2[out_len - 1];
	}
	return out_len;
}

/**
 * Simple square window FIR on real samples with decimation to rate_out2.
 *
 * @param s Demodulator state (uses result buffer and decimation state).
 */
void low_pass_real(struct demod_state *s)
{
	int i=0, i2=0;
	int16_t *r = assume_aligned_ptr(s->result, DSD_FME_ALIGN);
	int fast = (int)s->rate_out;
	int slow = s->rate_out2;
	/* Precompute fixed-point reciprocal of decimation factor to avoid per-sample division */
	int decim = (slow != 0) ? (fast / slow) : 1;
	if (decim < 1) decim = 1;
	const int kShiftLPR = 15; /* Q15 reciprocal */
	int recip_decim_q = (1 << kShiftLPR) / decim;
	DSD_FME_IVDEP
	while (i < s->result_len) {
		s->now_lpr += r[i];
		i++;
		s->prev_lpr_index += slow;
		if (s->prev_lpr_index < fast) {
			continue;
		}
		/* Multiply by reciprocal and shift instead of dividing by (fast/slow) */
		int64_t scaled = ((int64_t)s->now_lpr * recip_decim_q);
		r[i2] = (int16_t)(scaled >> kShiftLPR);
		s->prev_lpr_index -= fast;
		s->now_lpr = 0;
		i2 += 1;
	}
	s->result_len = i2;
}

/**
 * Fifth-order half-band-like decimator operating on a single real sequence.
 * Caller applies this separately to I and Q streams. Uses 6-tap state in
 * `hist` and writes decimated output in-place.
 *
 * @param data   In/out real data buffer (single channel).
 * @param length Input length (elements), processed in-place.
 * @param hist   Persistent history buffer of length >= 6.
 */
void fifth_order(int16_t *data, int length, int16_t *hist)
{
	int i;
	int16_t a, b, c, d, e, f;
	a = hist[1];
	b = hist[2];
	c = hist[3];
	d = hist[4];
	e = hist[5];
	f = data[0];
	/* a downsample should improve resolution, so don't fully shift */
	data[0] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	for (i=4; i<length; i+=4) {
		a = c;
		b = d;
		c = e;
		d = f;
		e = data[i-2];
		f = data[i];
		data[i/2] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	}
	/* archive */
	hist[0] = a;
	hist[1] = b;
	hist[2] = c;
	hist[3] = d;
	hist[4] = e;
	hist[5] = f;
}

/**
 * FIR filter with symmetric 9-tap coefficients (phase-saving implementation).
 *
 * @param data   In/out data buffer (interleaved step of 2 assumed).
 * @param length Number of input samples.
 * @param fir    Coefficient array (expects layout for length 9).
 * @param hist   History buffer used across calls.
 */
void generic_fir(int16_t *data, int length, int *fir, int16_t *hist)
{
	int d, temp, sum;
	for (d=0; d<length; d+=2) {
		temp = data[d];
		sum = 0;
		sum += (hist[0] + hist[8]) * fir[1];
		sum += (hist[1] + hist[7]) * fir[2];
		sum += (hist[2] + hist[6]) * fir[3];
		sum += (hist[3] + hist[5]) * fir[4];
		sum +=            hist[4]  * fir[5];
		data[d] = sum >> 15 ;
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = hist[3];
		hist[3] = hist[4];
		hist[4] = hist[5];
		hist[5] = hist[6];
		hist[6] = hist[7];
		hist[7] = hist[8];
		hist[8] = temp;
	}
}

/**
 * Complex multiply using 32-bit intermediates (suitable for small magnitudes).
 * Defined for platforms lacking hardware float.
 *
 * @param ar Real part of a.
 * @param aj Imag part of a.
 * @param br Real part of b.
 * @param bj Imag part of b.
 * @param cr [out] Real part of result.
 * @param cj [out] Imag part of result.
 */
void multiply(int ar, int aj, int br, int bj, int *cr, int *cj)
{
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

/**
 * Complex multiply using 64-bit intermediates to prevent overflow.
 *
 * @param ar Real part of a.
 * @param aj Imag part of a.
 * @param br Real part of b.
 * @param bj Imag part of b.
 * @param cr [out] Real part of result (int64).
 * @param cj [out] Imag part of result (int64).
 */
static inline void multiply64(int ar, int aj, int br, int bj, int64_t *cr, int64_t *cj)
{
	*cr = (int64_t)ar * (int64_t)br - (int64_t)aj * (int64_t)bj;
	*cj = (int64_t)aj * (int64_t)br + (int64_t)ar * (int64_t)bj;
}

int polar_discriminant(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	double angle;
	multiply64(ar, aj, br, -bj, &cr, &cj);
	angle = atan2((double)cj, (double)cr);
	return (int)(angle / kPi * (1<<14));
}

/**
 * Fast integer atan2 approximation pre-scaled for int16.
 *
 * @param y Imaginary component.
 * @param x Real component.
 * @return Angle where pi == 1<<14 (Q14 scaling).
 */
int fast_atan2(int y, int x)
{
	int yabs, angle;
	int pi4=(1<<12), pi34=3*(1<<12);  // note pi = 1<<14
	if (x==0 && y==0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		angle = pi4  - pi4 * (x-yabs) / (x+yabs);
	} else {
		angle = pi34 - pi4 * (x+yabs) / (yabs-x);
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

/**
 * 64-bit safe fast atan2 approximation to avoid overflow.
 *
 * @param y Imaginary component (int64).
 * @param x Real component (int64).
 * @return Angle where pi == 1<<14 (Q14 scaling).
 */
int fast_atan2_64(int64_t y, int64_t x)
{
	int angle;
	int pi4=(1<<12), pi34=3*(1<<12);  /* note: pi = 1<<14 */
	int64_t yabs;
	if (x == 0 && y == 0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		/* denominator (x + yabs) cannot be zero here unless x==y==0 handled above */
		angle = (int)(pi4  - ( (int64_t)pi4 * (x - yabs) ) / (x + yabs));
	} else {
		/* denominator (yabs - x) > 0 */
		angle = (int)(pi34 - ( (int64_t)pi4 * (x + yabs) ) / (yabs - x));
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

int polar_disc_fast(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	multiply64(ar, aj, br, -bj, &cr, &cj);
	return fast_atan2_64(cj, cr);
}

/**
 * Initialize the fast arctangent lookup table used by the LUT discriminator.
 * Thread-safe and idempotent; subsequent calls are inexpensive.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int atan_lut_init(void)
{
	/* Thread-safe, idempotent initialization */
	pthread_once(&atan_lut_once, atan_lut_once_init);
	if (atan_lut != NULL) {
		return 0;
	}
	/* If LUT was freed after once, allow re-init guarded by mutex */
	pthread_mutex_lock(&atan_lut_mutex);
	if (atan_lut == NULL) {
		atan_lut_once_init();
	}
	pthread_mutex_unlock(&atan_lut_mutex);
	return (atan_lut != NULL) ? 0 : -1;
}

/**
 * Free memory associated with the fast arctangent lookup table.
 * Safe to call multiple times.
 */
void atan_lut_free(void)
{
	pthread_mutex_lock(&atan_lut_mutex);
	if (atan_lut != NULL) {
		free(atan_lut);
		atan_lut = NULL;
	}
	pthread_mutex_unlock(&atan_lut_mutex);
}

/**
 * Polar discriminator using a lookup table for atan2 approximation.
 *
 * Multiplies sample b by the conjugate of sample a and estimates the phase
 * change via a LUT-backed atan2 approximation, returning a Q14-scaled angle.
 *
 * @param ar Real part of previous complex sample.
 * @param aj Imag part of previous complex sample.
 * @param br Real part of current complex sample.
 * @param bj Imag part of current complex sample.
 * @return Phase difference in Q14 where (pi == 1<<14).
 */
int polar_disc_lut(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	int64_t x, x_abs;

	/* Ensure LUT is available; fall back if allocation failed */
	atan_lut_init();
	if (atan_lut == NULL) {
		multiply64(ar, aj, br, -bj, &cr, &cj);
		return fast_atan2_64(cj, cr);
	}

	multiply64(ar, aj, br, -bj, &cr, &cj);

	/* special cases */
	if (cr == 0 || cj == 0) {
		if (cr == 0 && cj == 0)
			{return 0;}
		if (cr == 0 && cj > 0)
			{return 1 << 13;}
		if (cr == 0 && cj < 0)
			{return -(1 << 13);}
		if (cj == 0 && cr > 0)
			{return 0;}
		if (cj == 0 && cr < 0)
			{return (1 << 14) - 1;}
	}

	/* real range -32768 - 32768 use 64x range -> absolute maximum: 2097152 */
	x = ((int64_t)cj << atan_lut_coef) / cr;
	x_abs = (x < 0) ? -x : x;

	if (x_abs >= (int64_t)atan_lut_size) {
		/* Preserve quadrant using both cr and cj signs */
		if (cr < 0) {
			return (cj >= 0) ? ((1 << 14) - 1) : (-(1 << 14) + 1);
		} else {
			return (cj >= 0) ? (1 << 13) : -(1 << 13);
		}
	}

	if (x > 0) {
		int val = (cj > 0) ? atan_lut[(int)x] : (atan_lut[(int)x] - (1<<14));
		if (val == (1 << 14)) { val = (1 << 14) - 1; }
		if (val == -(1 << 14)) { val = -(1 << 14) + 1; }
		return val;
	} else {
		int val = (cj > 0) ? ((1<<14) - atan_lut[(int)(-x)]) : (-atan_lut[(int)(-x)]);
		if (val == (1 << 14)) { val = (1 << 14) - 1; }
		if (val == -(1 << 14)) { val = -(1 << 14) + 1; }
		return val;
	}

	return 0;
}

/**
 * Perform FM discriminator on interleaved low-passed I/Q to produce audio PCM.
 * Uses the active discriminator configured in fm->discriminator.
 *
 * @param fm Demodulator state (uses lowpassed as input, writes to result).
 */
void fm_demod(struct demod_state *fm)
{
	int i, pcm;
	int16_t *lp = assume_aligned_ptr(fm->lowpassed, DSD_FME_ALIGN);
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	/* Use selected discriminator from the very first sample */
	pcm = fm->discriminator(lp[0], lp[1], fm->pre_r, fm->pre_j);
	res[0] = (int16_t)pcm;
	DSD_FME_IVDEP
	for (i = 2; i < (fm->lp_len-1); i += 2) {
		pcm = fm->discriminator(lp[i], lp[i+1], lp[i-2], lp[i-1]);
		res[i/2] = (int16_t)pcm;
	}
	fm->pre_r = lp[fm->lp_len - 2];
	fm->pre_j = lp[fm->lp_len - 1];
	fm->result_len = fm->lp_len/2;
}

/**
 * Pass-through demodulator: copies low-passed samples to output unchanged.
 *
 * @param fm Demodulator state (copies lowpassed to result).
 */
void raw_demod(struct demod_state *fm)
{
	int i;
	for (i = 0; i < fm->lp_len; i++) {
		fm->result[i] = (int16_t)fm->lowpassed[i];
	}
	fm->result_len = fm->lp_len;
}

/**
 * Apply post-demod deemphasis IIR filter with Q15 coefficient.
 *
 * @param fm Demodulator state (reads/writes result, updates deemph_avg).
 */
void deemph_filter(struct demod_state *fm)
{
	int avg = fm->deemph_avg; /* per-instance state */
	int i, d;
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	/* Q15 alpha = (1 - a), where a = exp(-1/(Fs*tau)) */
	const int kShiftDeemph = 15; /* Q15 */
	int alpha_q15 = fm->deemph_a;
	if (alpha_q15 < 0) alpha_q15 = 0;
	if (alpha_q15 > (1 << kShiftDeemph)) alpha_q15 = (1 << kShiftDeemph);
	/* Single-pole IIR: avg += (x - avg) * alpha */
	DSD_FME_IVDEP
	for (i = 0; i < fm->result_len; i++) {
		d = res[i] - avg;
		int64_t delta = (int64_t)d * (int64_t)alpha_q15;
		/* symmetric rounding */
		if (d > 0) {
			delta += (1LL << (kShiftDeemph - 1));
		} else if (d < 0) {
			delta -= (1LL << (kShiftDeemph - 1));
		}
		avg += (int)(delta >> kShiftDeemph);
		res[i] = (int16_t)avg;
	}
	fm->deemph_avg = avg; /* write back state */
}

/**
 * Apply a simple DC blocking (leaky integrator high-pass) filter to audio.
 *
 * @param fm Demodulator state (reads/writes result, updates dc_avg).
 */
void dc_block_filter(struct demod_state *fm)
{
	int i;
	/* Leaky integrator high-pass: dc += (x - dc) >> k; y = x - dc */
	int dc = fm->dc_avg;
	const int k = 11; /* cutoff ~ Fs / 2^k (k in 10..12) */
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	DSD_FME_IVDEP
	for (i = 0; i < fm->result_len; i++) {
		int x = (int)res[i];
		dc += (x - dc) >> k;
		int y = x - dc;
		res[i] = sat16(y);
	}
	fm->dc_avg = dc;
}

/**
 * Optional light post-demod audio low-pass filter (one-pole IIR).
 * Implements: y[n] = y[n-1] + alpha * (x[n] - y[n-1]), where alpha is Q15 in
 * `fm->audio_lpf_alpha`.
 *
 * @param fm Demodulator state (reads/writes `result`, updates `audio_lpf_state`).
 */
static inline void audio_lpf_filter(struct demod_state *fm)
{
    if (!fm->audio_lpf_enable) return;
    int i;
    int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
    int y = fm->audio_lpf_state; /* Q0 */
    const int alpha_q15 = fm->audio_lpf_alpha; /* Q15 */
    const int kShift = 15;
    DSD_FME_IVDEP
    for (i = 0; i < fm->result_len; i++) {
        int x = (int)res[i];
        int d = x - y;
        int64_t delta = (int64_t)d * (int64_t)alpha_q15;
        /* symmetric rounding */
        if (d >= 0) delta += (1LL << (kShift - 1));
        else        delta -= (1LL << (kShift - 1));
        y += (int)(delta >> kShift);
        res[i] = (int16_t)y;
    }
    fm->audio_lpf_state = y;
}

/**
 * Mix lowpassed I/Q by NCO e^{j*phi}, update phase by `fll_freq_q15` per sample.
 * Phase and frequency are Q15 where a full turn (2*pi) maps to 1<<15.
 *
 * @param d Demodulator state (reads/writes `lowpassed`, updates `fll_phase_q15`).
 */
static inline void fll_mix_and_update(struct demod_state *d)
{
    if (!d->fll_enabled) return;
    int16_t *x = d->lowpassed;
    const int N = d->lp_len;
    int phase = d->fll_phase_q15;   /* Q15 wraps at 1<<15 ~ 2*pi */
    const int freq = d->fll_freq_q15; /* Q15 increment per sample */
    /* Optional: higher-quality quarter-wave LUT rotator (linear interp), enabled via DSD_FME_FLL_LUT=1. */
    if (fll_lut_enabled) {
    	/* Ensure LUT is initialized once before use */
    	pthread_once(&fll_lut_once, fll_lut_once_init);
    	for (int i = 0; i + 1 < N; i += 2) {
    		int16_t c, s;
    		fll_sin_cos_q15_from_phase_lut(phase, &c, &s);
    		int xr = x[i];
    		int xj = x[i+1];
    		int32_t yr = ((int32_t)xr * c + (int32_t)xj * s) >> 15;
    		int32_t yj = ((int32_t)xj * c - (int32_t)xr * s) >> 15;
    		x[i]   = (int16_t)yr;
    		x[i+1] = (int16_t)yj;
    		phase += freq;
    	}
    } else {
    	/* Fast LUT-free rotator: piecewise-linear sin/cos within quadrants. */
    	for (int i = 0; i + 1 < N; i += 2) {
    		int p = phase & 0x7FFF; /* 0..32767 */
    		int q = p >> 13; /* quadrant 0..3 */
    		int16_t c = 32767, s = 0;
    		int16_t r = (int16_t)(p & 0x1FFF); /* 0..8191 */
    		switch (q) {
    			case 0: c = 32767; s = (int16_t)((r * 4)); break;
    			case 1: c = (int16_t)(32767 - (r * 4)); s = 32767; break;
    			case 2: c = -32767; s = (int16_t)(32767 - (r * 4)); break;
    			default: c = (int16_t)(-32767 + (r * 4)); s = -32767; break;
    		}
    		int xr = x[i];
    		int xj = x[i+1];
    		int32_t yr = ((int32_t)xr * c + (int32_t)xj * s) >> 15;
    		int32_t yj = ((int32_t)xj * c - (int32_t)xr * s) >> 15;
    		x[i]   = (int16_t)yr;
    		x[i+1] = (int16_t)yj;
    		phase += freq;
    	}
    }
    d->fll_phase_q15 = phase & 0x7FFF;
}

/**
 * Estimate frequency error using a simple phase-difference discriminator and
 * update the FLL control in Q15. The proportional term is applied directly
 * and the integral action is realized by accumulating into `fll_freq_q15`.
 *
 * @param d Demodulator state (updates `fll_freq_q15` and `fll_phase_q15`).
 */
static inline void fll_update_error(struct demod_state *d)
{
    if (!d->fll_enabled) return;
    int16_t *x = d->lowpassed;
    const int N = d->lp_len;
    int alpha = d->fll_alpha_q15; /* Q15 */
    int beta  = d->fll_beta_q15;  /* Q15 */
    int prev_r = d->fll_prev_r;
    int prev_j = d->fll_prev_j;
    int32_t err_acc = 0;
    int count = 0;
    for (int i = 0; i + 1 < N; i += 2) {
        int r = x[i];
        int j = x[i+1];
        if (i > 0 || (prev_r != 0 || prev_j != 0)) {
            int e = polar_disc_fast(r, j, prev_r, prev_j); /* Q14 */
            err_acc += e;
            count++;
        }
        prev_r = r; prev_j = j;
    }
    d->fll_prev_r = prev_r;
    d->fll_prev_j = prev_j;
    if (count == 0) return;
    int32_t err = err_acc / count; /* Q14 */
    int32_t p = ( (int64_t)alpha * err ) >> 14; /* Q14 */
    int32_t iacc = ( (int64_t)beta  * err ) >> 14;
    int32_t df = p + iacc;
    d->fll_freq_q15 += (int)df; /* Q15 */
}

/**
 * Lightweight Gardner timing correction.
 * Uses linear interpolation between adjacent complex samples around the
 * nominal samples-per-symbol to reduce timing error; intended for digital
 * modes when enabled.
 *
 * @param d Demodulator state (may adjust `result` in-place).
 */
static inline void gardner_timing_adjust(struct demod_state *d)
{
    if (!d->ted_enabled || d->ted_sps <= 1) return;
    /* Guard: run TED only when we're near symbol rate to keep CPU low.
       Skip when samples-per-symbol is very high unless explicitly forced. */
    int sps = d->ted_sps;
    if (sps > 12 && !d->ted_force) return;
    int mu  = d->ted_mu_q20;    /* Q20 */
    int gain = d->ted_gain_q20; /* Q20 */
    int16_t *x = d->lowpassed;
    int16_t *y = d->timing_buf;
    const int N = d->lp_len;
    int out_n = 0;
    const int one = (1<<20);
    int mu_nom = one / (sps > 0 ? sps : 1); /* Q20 increment per complex sample */
    for (int n = 0; n + 3 < N; n += 2) {
        int a = n;       /* base complex sample index */
        int b = n + 2;   /* next complex sample index */
        if (b + 1 >= N) break;
        int frac = mu & (one - 1); /* 0..one-1 */
        int inv  = one - frac;
        /* Linear interpolation between x[a] and x[b] (complex) */
        int32_t ar = x[a];   int32_t aj = x[a+1];
        int32_t br = x[b];   int32_t bj = x[b+1];
        int32_t ir = (int32_t)(( (int64_t)inv * ar + (int64_t)frac * br ) >> 20);
        int32_t ij = (int32_t)(( (int64_t)inv * aj + (int64_t)frac * bj ) >> 20);
        y[out_n++] = (int16_t)ir;
        y[out_n++] = (int16_t)ij;

        /* Gardner error using previous and next symbol-spaced samples */
        int km1 = a - 2; if (km1 < 0) km1 = 0;
        int kp1 = b + 2; if (kp1 + 1 >= N) { kp1 = b; }
        int16_t xr1 = x[kp1];
        int16_t xj1 = x[kp1+1];
        int16_t xrm = x[km1];
        int16_t xjm = x[km1+1];
        int16_t dr = xr1 - xrm;
        int16_t dj = xj1 - xjm;
        int32_t e = (int32_t)dr * (int32_t)ir + (int32_t)dj * (int32_t)ij; /* Q0 */

        /* Update fractional phase: nominal advance + small correction */
        int64_t corr = ((int64_t)gain * (int64_t)e) >> 15; /* scale */
        mu += mu_nom + (int)corr;
        /* Wrap mu to [0, one) */
        if (mu >= one) mu -= one;
        if (mu < 0) mu += one;
    }
    if (out_n >= 2) {
        memcpy(d->lowpassed, y, (size_t)out_n * sizeof(int16_t));
        d->lp_len = out_n;
    }
    d->ted_mu_q20 = mu;
}

/**
 * Greatest common divisor via Euclidean algorithm.
 */
static inline int gcd_int(int a, int b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return (a == 0) ? 1 : a;
}

/**
 * Normalized sinc function: sin(pi*x)/(pi*x), with sinc(0)=1.
 */
static inline double dsd_fme_sinc(double x)
{
    if (x == 0.0) return 1.0;
    return sin(kPi * x) / (kPi * x);
}

/**
 * Design windowed-sinc low-pass prototype for polyphase upfirdn (runs at L*Fs_in).
 * Uses a Hamming window and conservative cutoff to balance CPU vs. stopband.
 * Taps are stored phase-major with stride L (k*L + phase) so each phase sees
 * a contiguous sub-filter. Taps are normalized to give ~unity DC per phase,
 * then scaled by L to compensate polyphase upsampling (maintains amplitude).
 *
 * @param s Demodulator state to receive resampler taps/history.
 * @param L Upsampling factor.
 * @param M Downsampling factor.
 */
static void resamp_design(struct demod_state *s, int L, int M)
{
    /* Per-phase taps K; total taps = K*L. Keep small for CPU, but adequate stopband. */
    int taps_per_phase = 16; /* K */
    int total_taps = taps_per_phase * L;
    if (total_taps < L) total_taps = L;
    if (taps_per_phase < 8) taps_per_phase = 8;

    /* Normalized cutoff: conservative 0.45 * min(1/L, 1/M) of Nyquist at L*Fs. */
    double fc = 0.45 / (double)((L > M) ? L : M); /* 0..0.5 at L*Fs */
    int N = total_taps;
    int mid = (N - 1) / 2;

    /* Allocate taps if needed */
    if (s->resamp_taps) { free(s->resamp_taps); s->resamp_taps = NULL; }
    if (s->resamp_hist) { free(s->resamp_hist); s->resamp_hist = NULL; }
    s->resamp_taps = static_cast<int16_t*>(malloc((size_t)N * sizeof(int16_t)));
    s->resamp_hist = static_cast<int16_t*>(malloc((size_t)taps_per_phase * sizeof(int16_t)));
    if (!s->resamp_taps || !s->resamp_hist) {
        if (s->resamp_taps) { free(s->resamp_taps); s->resamp_taps = NULL; }
        if (s->resamp_hist) { free(s->resamp_hist); s->resamp_hist = NULL; }
        s->resamp_enabled = 0;
        fprintf(stderr, "Rational resampler: allocation failed, disabling.\n");
        return;
    }
    memset(s->resamp_hist, 0, (size_t)taps_per_phase * sizeof(int16_t));
    s->resamp_hist_head = 0;

    /* Windowed-sinc (Hamming) */
    double gain = 0.0;
    for (int n = 0; n < N; n++) {
        int m = n - mid;
        double w = 0.54 - 0.46 * cos(2.0 * kPi * (double)n / (double)(N - 1));
        double h = 2.0 * fc * dsd_fme_sinc(2.0 * fc * (double)m);
        double t = h * w;
        gain += t;
    }
    /* Normalize to unity DC gain */
    if (gain == 0.0) gain = 1.0;
    /* Compensate for polyphase upsampling: scale taps by L so that each phase
       has approximately unity DC gain (preserves amplitude through upfirdn). */
    const double phase_gain_comp = (double)L;
    for (int n = 0; n < N; n++) {
        int m = n - mid;
        double w = 0.54 - 0.46 * cos(2.0 * kPi * (double)n / (double)(N - 1));
        double h = 2.0 * fc * dsd_fme_sinc(2.0 * fc * (double)m);
        double t = (h * w / gain) * phase_gain_comp;
        int v = (int)lrint(t * (double)(1 << 15));
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        s->resamp_taps[n] = (int16_t)v;
    }

    s->resamp_L = L;
    s->resamp_M = M;
    s->resamp_phase = 0;
    s->resamp_taps_len = N;
    s->resamp_taps_per_phase = taps_per_phase;
}

/**
 * Process one block using polyphase upfirdn with history.
 *
 * @param s      Demodulator state containing resampler state.
 * @param in     Pointer to input samples.
 * @param in_len Number of input samples.
 * @param out    Pointer to output buffer (sized to hold produced samples).
 * @return Number of output samples written.
 */
static int resamp_process_block(struct demod_state *s, const int16_t *in, int in_len, int16_t *out)
{
    if (!s->resamp_enabled || !s->resamp_taps || !s->resamp_hist) {
        /* passthrough */
        memcpy(out, in, (size_t)in_len * sizeof(int16_t));
        return in_len;
    }
    const int L = s->resamp_L;
    const int M = s->resamp_M;
    const int K = s->resamp_taps_per_phase; /* taps per phase */
    const int16_t *taps = s->resamp_taps;   /* length K*L, phase-major stride L */
    int phase = s->resamp_phase;            /* 0..L-1 */
    int head = s->resamp_hist_head;         /* circular head index */
    int out_len = 0;

    for (int n = 0; n < in_len; n++) {
        /* Push new sample into circular history at head */
        s->resamp_hist[head] = in[n];
        head++;
        if (head == K) head = 0;

        /* While we owe outputs with current input available */
        int local_phase = phase;
        while (local_phase < L) {
            /* Dot: y = sum_{k=0..K-1} hist[idx] * taps[k*L + local_phase]
               Access history most-recent-first starting from head-1. */
            int64_t acc = 0;
            const int16_t *tk = taps + local_phase;
            int idx = head - 1; if (idx < 0) idx += K;
            for (int k = 0; k < K; k++) {
                acc += (int32_t)s->resamp_hist[idx] * (int32_t)tk[0];
                tk += L;
                idx--; if (idx < 0) idx += K;
            }
            /* Q15 -> Q0 with rounding and saturation */
            acc += (1 << 14);
            int32_t y = (int32_t)(acc >> 15);
            out[out_len++] = sat16(y);
            local_phase += M;
        }
        phase = local_phase - L;
    }

    s->resamp_phase = phase;
    s->resamp_hist_head = head;
    return out_len;
}

/**
 * DC-corrected mean power (sqrt-free). Integer-only implementation.
 *
 * @param samples Input sample buffer.
 * @param len     Number of samples to process.
 * @param step    Step between processed samples (subsampling).
 * @return Mean power (squared RMS) with DC bias removed.
 */
long int mean_power(int16_t *samples, int len, int step)
{
	int64_t p = 0;
	int64_t t = 0;
	for (int i = 0; i < len; i += step) {
		int64_t s = (int64_t)samples[i];
		t += s;
		p += s * s;
	}
	/* DC-corrected energy ≈ p - (t^2)/len with rounded division */
	int64_t dc_corr = 0;
	if (len > 0) {
		int64_t tt = t * t;
		dc_corr = (tt + (len / 2)) / len;
	}
	int64_t energy = p - dc_corr;
	if (energy < 0) energy = 0;
	return (long int)(energy / (len > 0 ? len : 1));
}

/**
 * Full demodulation pipeline for one block.
 * Applies decimation (HB cascade or legacy), optional FLL and timing
 * correction, followed by the configured discriminator and post-processing.
 *
 * @param d Demodulator state (consumes lowpassed, produces result).
 */
void full_demod(struct demod_state *d)
{
	int i, ds_p;
	ds_p = d->downsample_passes;
	if (ds_p) {
		/* Choose decimator: half-band cascade (default) or legacy path */
		if (use_halfband_decimator) {
			/* Apply ds_p stages of 2:1 half-band decimation on interleaved lowpassed */
			int in_len = d->lp_len;
			int16_t *src = d->lowpassed;
			int16_t *dst = d->hb_workbuf;
			for (i = 0; i < ds_p; i++) {
				/* Fused complex HB decimation on interleaved I/Q */
				int out_len_interleaved = hb_decim2_complex_interleaved(src, in_len, dst, d->hb_hist_i[i], d->hb_hist_q[i]);
				/* Next stage */
				src = dst;
				in_len = out_len_interleaved;
				dst = (src == d->hb_workbuf) ? d->lowpassed : d->hb_workbuf;
			}
			/* Final output resides in 'src' with length in_len; consume in-place (no copy) */
			d->lowpassed = src;
			d->lp_len = in_len;
			/* No droop compensation for half-band cascade */
		} else {
			for (i=0; i < ds_p; i++) {
				fifth_order(d->lowpassed,   (d->lp_len >> i), d->lp_i_hist[i]);
				fifth_order(d->lowpassed+1, (d->lp_len >> i) - 1, d->lp_q_hist[i]);
			}
			d->lp_len = d->lp_len >> ds_p;
			/* droop compensation */
			if (d->comp_fir_size == 9 && ds_p <= CIC_TABLE_MAX) {
				generic_fir(d->lowpassed, d->lp_len,
					cic_9_tables[ds_p], d->droop_i_hist);
				generic_fir(d->lowpassed+1, d->lp_len-1,
					cic_9_tables[ds_p], d->droop_q_hist);
			}
		}
	} else {
		low_pass(d);
	}
	/* Residual CFO correction before discriminator */
	fll_mix_and_update(d);
	/* Lightweight timing error correction (optional, avoid for analog FM demod) */
	if (d->ted_enabled && (d->mode_demod != &fm_demod || d->ted_force)) {
		gardner_timing_adjust(d);
	}
	/* Power squelch (sqrt-free): compare mean power estimate against squared threshold.
	   Samples are decimated by `squelch_decim_stride`; an EMA smooths block power.
	   The sampling phase advances by lp_len % stride per block to cover all offsets. */
	if (d->squelch_level) {
		/* Decimated block power estimate (no DC correction; EMA smooths) */
		int stride = (d->squelch_decim_stride > 0) ? d->squelch_decim_stride : 16;
		int phase = d->squelch_decim_phase;
		int64_t p = 0;
		int count = 0;
		for (int j = phase; j < d->lp_len; j += stride) {
			int64_t s2 = (int64_t)d->lowpassed[j];
			p += s2 * s2;
			count++;
		}
		/* Advance phase to sample different positions next block */
		if (stride > 0) {
			int adv = d->lp_len % stride;
			d->squelch_decim_phase = (phase + adv) % stride;
		}
		if (count > 0) {
			int64_t block_mean = p / count;
			if (d->squelch_running_power == 0) {
				/* Initialize on first measurement to avoid long ramp */
				d->squelch_running_power = block_mean;
			} else {
				/* EMA: running += (block_mean - running) / window, window ~ 2^shift */
				int w = (d->squelch_window > 0) ? d->squelch_window : 2048;
				int shift = 0;
				/* approximate log2(window), prefer power-of-two windows */
				while ((1 << shift) < w && shift < 30) { shift++; }
				int64_t delta = (block_mean - d->squelch_running_power);
				d->squelch_running_power += (delta >> shift);
			}
		}
		int64_t thr2 = (int64_t)d->squelch_level * (int64_t)d->squelch_level;
		if (d->squelch_running_power < thr2) {
			d->squelch_hits++;
			for (i=0; i<d->lp_len; i++) {
				d->lowpassed[i] = 0;
			}
		} else {
			d->squelch_hits = 0;
		}
	}
	d->mode_demod(d);  /* lowpassed -> result */
	if (d->mode_demod == &raw_demod) {
		return;
	}
	/* todo, fm noise squelch */
	// use nicer filter here too?
	if (d->post_downsample > 1) {
		d->result_len = low_pass_simple(d->result, d->result_len, d->post_downsample);}
	if (d->deemph) {
		deemph_filter(d);} 
	/* Optional post-demod audio LPF */
	audio_lpf_filter(d);
	if (d->dc_block) {
		dc_block_filter(d);}
	if (d->rate_out2 > 0) {
		low_pass_real(d);
		//arbitrary_resample(d->result, d->result, d->result_len, d->result_len * d->rate_out2 / d->rate_out);
	}
}

/**
 * RTL-SDR asynchronous USB callback.
 * Converts incoming u8 I/Q to s16 and enqueues into the input ring. If
 * `offset_tuning` is off and `DSD_FME_COMBINE_ROT` is enabled (default), a
 * combined rotate+widen implementation is used. Otherwise it falls back to
 * legacy two-pass (rotate_90 u8, then widen subtracting 128) or a simple
 * widen subtracting 127. On overflow, drops oldest ring data to avoid stalls.
 *
 * @param buf USB I/Q byte buffer.
 * @param len Buffer length in bytes (I/Q interleaved).
 * @param ctx Opaque pointer to `dongle_state`.
 */
static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	
	struct dongle_state *s = static_cast<dongle_state*>(ctx);
	/* One-time: ensure the USB callback thread gets RT scheduling/affinity if enabled */
	{
		static std::atomic<int> usb_sched_applied{0};
		int expected = 0;
		if (usb_sched_applied.compare_exchange_strong(expected, 1)) {
			maybe_set_thread_realtime_and_affinity("USB");
		}
	}

	if (exitflag) {
		return;}
	if (!ctx) {
		return;}
	if (s->mute) {
		/* Clamp mute length to buffer size to avoid overwrite; carry remainder */
		int old = s->mute.load(std::memory_order_relaxed);
		if (old > 0) {
			uint32_t m = (uint32_t)old;
			if (m > len) m = len;
			memset(buf, 127, m);
			s->mute.fetch_sub((int)m, std::memory_order_relaxed);
		}
	}
	/* Convert incoming u8 I/Q and write directly into input ring without extra copy */
	size_t need = len;
	size_t done = 0;
	/* For legacy two-pass path, rotate the incoming byte buffer once up front */
	int use_two_pass = (!s->offset_tuning && !combine_rotate_enabled);
	if (use_two_pass) {
		rotate_90(buf, len);
	}
	while (need > 0) {
		int16_t *p1 = NULL, *p2 = NULL; size_t n1 = 0, n2 = 0;
		input_ring_reserve(&input_ring, need, &p1, &n1, &p2, &n2);
		if (n1 == 0 && n2 == 0) {
			/* Ring still full after drop attempt; give up this callback to avoid stall */
			break;
		}
		/* Ensure even counts to keep I/Q pairs aligned */
		if (n1 & 1) n1--;
		size_t w1 = (n1 < need) ? n1 : need;
		size_t rem_after_w1 = need - w1;
		if (n2 & 1) n2--;
		size_t w2 = (n2 < rem_after_w1) ? n2 : rem_after_w1;

		if (!s->offset_tuning && combine_rotate_enabled) {
			if (w1) widen_rotate90_u8_to_s16_bias127(buf + done, p1, (uint32_t)w1);
			if (w2) widen_rotate90_u8_to_s16_bias127(buf + done + w1, p2, (uint32_t)w2);
		} else if (use_two_pass) {
			/* bytes already rotated in-place; widen with 128 subtraction to avoid bias */
			if (w1) widen_u8_to_s16_bias128_scalar(buf + done, p1, (uint32_t)w1);
			if (w2) widen_u8_to_s16_bias128_scalar(buf + done + w1, p2, (uint32_t)w2);
		} else {
			if (w1) widen_u8_to_s16_bias127(buf + done, p1, (uint32_t)w1);
			if (w2) widen_u8_to_s16_bias127(buf + done + w1, p2, (uint32_t)w2);
		}
		input_ring_commit(&input_ring, w1 + w2);
		done += w1 + w2;
		need -= w1 + w2;
	}
}

/**
 * RTL-SDR USB thread entry: reads samples asynchronously into the input ring.
 * Applies optional realtime scheduling/affinity if configured.
 *
 * @param arg Pointer to `dongle_state`.
 * @return NULL on exit.
 */
static void *dongle_thread_fn(void *arg)
{
	struct dongle_state *s = static_cast<dongle_state*>(arg);
	maybe_set_thread_realtime_and_affinity("DONGLE");
	rtlsdr_read_async(s->dev, rtlsdr_callback, s, 16, s->buf_len);
	return 0;
}

/**
 * Demodulation thread entry: reads from input ring, runs the demod pipeline,
 * and writes audio samples to the output ring.
 *
 * @param arg Pointer to `demod_state`.
 * @return NULL on exit.
 */
static void *demod_thread_fn(void *arg)
{
	struct demod_state *d = static_cast<demod_state*>(arg);
	struct output_state *o = d->output_target;
	maybe_set_thread_realtime_and_affinity("DEMOD");
	while (!exitflag) {
		/* Read a block from input ring */
		int got = input_ring_read_block(&input_ring, d->input_cb_buf, MAXIMUM_BUF_LENGTH);
		if (got <= 0) continue;
		d->lowpassed = d->input_cb_buf;
		d->lp_len = got;
		full_demod(d);
		if (d->exit_flag) {
			exitflag = 1;
		}
		if (d->squelch_level && d->squelch_hits > d->conseq_squelch) {
			d->squelch_hits = d->conseq_squelch + 1;  /* hair trigger */
			safe_cond_signal(&controller.hop, &controller.hop_m);
			continue;
		}
		/* Preferred path: rational resampler when enabled; otherwise legacy upsampler */
		if (d->resamp_enabled) {
			int out_n = resamp_process_block(d, d->result, d->result_len, d->resamp_outbuf);
			if (out_n > 0) ring_write_signal_on_empty_transition(o, d->resamp_outbuf, (size_t)out_n);
		} else {
			/* Legacy path: optional simple upsampler */
			if (bandwidth_multiplier <= 1) {
				ring_write_signal_on_empty_transition(o, d->result, (size_t)d->result_len);
			} else {
				const int M = bandwidth_multiplier;
				const int N = d->result_len;
				if (N <= 0) {
					/* nothing to write */
				} else if (N == 1) {
					for (int m = 0; m < M; m++) d->upsample_buf[m] = d->result[0];
					ring_write_signal_on_empty_transition(o, d->upsample_buf, (size_t)M);
				} else {
					const size_t up_len = (size_t)N * (size_t)M;
					struct UpArg { int start; int end; int M; const int16_t *src; int16_t *dst; };
					auto up_task = [](void *arg){
						UpArg *a = (UpArg*)arg;
						const int Mloc = a->M;
						for (int n = a->start; n < a->end; n++) {
							int32_t x0 = a->src[n];
							int32_t x1 = a->src[n + 1];
							int16_t *row = a->dst + (size_t)n * (size_t)Mloc;
							if (upsample_fixedpoint_enabled) {
								int32_t dx = x1 - x0;
								int64_t step_q15_64 = ((int64_t)dx << 15) / (int64_t)Mloc;
								int32_t step_q15 = (int32_t)step_q15_64;
								int32_t acc_q15 = 0;
								for (int m = 0; m < Mloc; m++) {
									int32_t frac = (acc_q15 >= 0) ? ((acc_q15 + (1 << 14)) >> 15) : -(((-acc_q15) + (1 << 14)) >> 15);
									int32_t interp = x0 + frac;
									row[m] = (int16_t)interp;
									acc_q15 += step_q15;
								}
							} else {
								int32_t dx = x1 - x0;
								for (int m = 0; m < Mloc; m++) {
									int32_t interp = x0 + (dx * m) / Mloc;
									row[m] = (int16_t)interp;
								}
							}
						}
					};
					int mid = (N - 1) / 2;
					UpArg a0 = {0, mid, M, d->result, d->upsample_buf};
					UpArg a1 = {mid, N - 1, M, d->result, d->upsample_buf};
					if (d->mt_enabled) {
						demod_mt_run_two(d, up_task, (void*)&a0, up_task, (void*)&a1);
					} else {
						up_task((void*)&a0);
						up_task((void*)&a1);
					}
					d->upsample_buf[(size_t)(N - 1) * (size_t)M] = d->result[N - 1];
					if (upsample_fixedpoint_enabled) {
						for (int t = 1; t < M; t++) {
							d->upsample_buf[(size_t)(N - 1) * (size_t)M + (size_t)t] = d->result[N - 1];
						}
					}
					ring_write_signal_on_empty_transition(o, d->upsample_buf, up_len);
				}
			}
		}
		/* Signaling occurs only when the ring transitions from empty to non-empty. */
	}
	return 0;
}

/**
 * Find the nearest supported tuner gain to the requested value.
 *
 * @param dev          RTL-SDR device handle.
 * @param target_gain  Desired gain in tenths of dB.
 * @return Nearest supported gain in tenths of dB, or a negative error code.
 */
int nearest_gain(rtlsdr_dev_t *dev, int target_gain)
{
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = static_cast<int*>(malloc(sizeof(int) * count));
	count = rtlsdr_get_tuner_gains(dev, gains);
	nearest = gains[0];
	for (i=0; i<count; i++) {
		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			nearest = gains[i];
		}
	}
	free(gains);
	return nearest;
}

/**
 * Set RTL-SDR center frequency with a brief status message.
 *
 * @param dev       RTL-SDR device handle.
 * @param frequency Center frequency in Hz.
 * @return 0 on success or a negative error code.
 */
int verbose_set_frequency(rtlsdr_dev_t *dev, uint32_t frequency)
{
	int r;
	r = rtlsdr_set_center_freq(dev, frequency);
	if (r < 0) {
		fprintf (stderr, " (WARNING: Failed to set Center Frequency). \n");
	} else {
		fprintf (stderr, " (Center Frequency: %u Hz.) \n", frequency);
	}
	return r;
}

/**
 * Set RTL-SDR sampling rate with a brief status message.
 *
 * @param dev       RTL-SDR device handle.
 * @param samp_rate Sampling rate in Hz.
 * @return 0 on success or a negative error code.
 */
int verbose_set_sample_rate(rtlsdr_dev_t *dev, uint32_t samp_rate)
{
	int r;
	r = rtlsdr_set_sample_rate(dev, samp_rate);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to set sample rate.\n");
	} else {
		fprintf (stderr, "Sampling at %u S/s.\n", samp_rate);
	}
	return r;
}

/**
 * Enable or disable direct sampling mode.
 *
 * @param dev RTL-SDR device handle.
 * @param on  Non-zero to enable, zero to disable.
 * @return 0 on success or a negative error code.
 */
int verbose_direct_sampling(rtlsdr_dev_t *dev, int on)
{
	int r;
	r = rtlsdr_set_direct_sampling(dev, on);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set direct sampling mode.\n");
		return r;
	}
	if (on == 0) {
		fprintf (stderr, "Direct sampling mode disabled.\n");}
	if (on == 1) {
		fprintf (stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (on == 2) {
		fprintf (stderr, "Enabled direct sampling mode, input 2/Q.\n");}
	return r;
}

/**
 * Enable offset tuning on the tuner if supported.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success or a negative error code.
 */
int verbose_offset_tuning(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_set_offset_tuning(dev, 1);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set offset tuning.\n");
	} else {
		fprintf (stderr, "Offset tuning mode enabled.\n");
	}
	return r;
}

/**
 * Enable tuner automatic gain control.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success or a negative error code.
 */
int verbose_auto_gain(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf (stderr, "Tuner gain set to automatic.\n");
	}
	return r;
}

/**
 * Set a fixed tuner gain with a message indicating the result.
 *
 * @param dev  RTL-SDR device handle.
 * @param gain Desired gain in tenths of dB.
 * @return 0 on success or a negative error code.
 */
int verbose_gain_set(rtlsdr_dev_t *dev, int gain)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	r = rtlsdr_set_tuner_gain(dev, gain);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf (stderr, "Tuner gain set to %0.2f dB.\n", gain/10.0);
	}
	return r;
}

/**
 * Set tuner PPM frequency error correction.
 *
 * @param dev        RTL-SDR device handle.
 * @param ppm_error  Error in parts-per-million.
 * @return 0 on success or a negative error code.
 */
int verbose_ppm_set(rtlsdr_dev_t *dev, int ppm_error)
{
	int r;
	// if (ppm_error == 0) {
	// 	return 0;}
	r = rtlsdr_set_freq_correction(dev, ppm_error);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to set ppm error.\n");
	} else {
		fprintf (stderr, "Tuner error set to %i ppm.\n", ppm_error);
	}
	return r;
}

/**
 * Reset RTL-SDR USB buffers.
 *
 * @param dev RTL-SDR device handle.
 * @return 0 on success or a negative error code.
 */
int verbose_reset_buffer(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to reset buffers.\n");}
	return r;
}

/**
 * Compute and stage tuner/demodulator capture settings based on the
 * requested center frequency and current demod configuration. The actual
 * device programming occurs elsewhere after these fields are updated.
 *
 * @param freq Desired RF center frequency in Hz.
 * @param rate Current input sample rate (unused).
 */
static void optimal_settings(int freq, int rate)
{
	UNUSED(rate);

	// giant ball of hacks
	// seems unable to do a single pass, 2:1
	int capture_freq, capture_rate;
	struct dongle_state *d = &dongle;
	struct demod_state *dm = &demod;
	struct controller_state *cs = &controller;
	dm->downsample = (1000000 / dm->rate_in) + 1; //dm->rate_in is the rtl_bandwidth value
	if (dm->downsample_passes) {
		dm->downsample_passes = (int)log2(dm->downsample) + 1;
		dm->downsample = 1 << dm->downsample_passes;
	}
	capture_freq = freq;
	capture_rate = dm->downsample * dm->rate_in; //
	if (!d->offset_tuning) {
		capture_freq = freq + capture_rate/4;} //
	capture_freq += cs->edge * dm->rate_in / 2;
	dm->output_scale = (1<<15) / (128 * dm->downsample);
	if (dm->output_scale < 1) {
		dm->output_scale = 1;}
	if (dm->mode_demod == &fm_demod) {
		dm->output_scale = 1;}
	d->freq = (uint32_t)capture_freq;
	d->rate = (uint32_t)capture_rate;
	// fprintf (stderr, "Capture Frequency: %i Rate: %i \n", capture_freq, capture_rate);
}

/**
 * Controller thread: handles basic scanning/hopping between channels.
 *
 * @param arg Pointer to `controller_state`.
 * @return NULL on exit.
 */
static void *controller_thread_fn(void *arg)
{
	// thoughts for multiple dongles
	// might be no good using a controller thread if retune/rate blocks
	int i;
	struct controller_state *s = static_cast<controller_state*>(arg);

	if (s->wb_mode) {
		for (i=0; i < s->freq_len; i++) {
			s->freqs[i] += 16000;}
	}

	/* set up primary channel */
	optimal_settings(s->freqs[0], demod.rate_in);
	if (dongle.direct_sampling) {
		verbose_direct_sampling(dongle.dev, 1);}
	if (dongle.offset_tuning) {
		verbose_offset_tuning(dongle.dev);}

	/* Set the frequency */
	verbose_set_frequency(dongle.dev, dongle.freq);
	fprintf (stderr, "Oversampling input by: %ix.\n", demod.downsample);
	fprintf (stderr, "Oversampling output by: %ix.\n", demod.post_downsample);
	fprintf (stderr, "Buffer size: %0.2fms\n",
		1000 * 0.5 * (float)ACTUAL_BUF_LENGTH / (float)dongle.rate);

	/* Set the sample rate */
	verbose_set_sample_rate(dongle.dev, dongle.rate);
	fprintf (stderr, "Output at %u Hz.\n", demod.rate_in/demod.post_downsample);

	while (!exitflag) {
		safe_cond_wait(&s->hop, &s->hop_m);
		if (s->freq_len <= 1) {
			continue;}
		/* hacky hopping */
		s->freq_now = (s->freq_now + 1) % s->freq_len;
		optimal_settings(s->freqs[s->freq_now], demod.rate_in);
		rtlsdr_set_center_freq(dongle.dev, dongle.freq);
		dongle.mute = BUFFER_DUMP;
	}
	return 0;
}

/**
 * Initialize dongle (RTL-SDR source) state with default parameters.
 *
 * @param s Dongle state to initialize.
 */
void dongle_init(struct dongle_state *s)
{
	s->rate = rtl_bandwidth;
	s->gain = AUTO_GAIN; // tenths of a dB
	s->mute = 0;
	s->direct_sampling = 0;
	s->offset_tuning = 0; //E4000 tuners only
	s->demod_target = &demod;
}

/**
 * Initialize demodulator state for analog FM path.
 *
 * @param s Demodulator state to initialize.
 */
void demod_init_analog(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 1; //
	s->comp_fir_size = 9;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 1;
	s->deemph = 1; //
	s->rate_out2 = rtl_bandwidth;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0; //
	s->deemph_avg = 0;
	/* Audio LPF defaults */
	s->audio_lpf_enable = 0;
	s->audio_lpf_alpha = 0;
	s->audio_lpf_state = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //
	s->dc_avg = 0;
	/* Resampler defaults */
	s->resamp_enabled = 0;
	s->resamp_target_hz = 0;
	s->resamp_L = 1;
	s->resamp_M = 1;
	s->resamp_phase = 0;
	s->resamp_taps_len = 0;
	s->resamp_taps_per_phase = 0;
	s->resamp_taps = NULL;
	s->resamp_hist = NULL;
	/* FLL/TED defaults */
	s->fll_enabled = 0;
	s->fll_alpha_q15 = 0;
	s->fll_beta_q15 = 0;
	s->fll_freq_q15 = 0;
	s->fll_phase_q15 = 0;
	s->fll_prev_r = 0;
	s->fll_prev_j = 0;
	s->ted_enabled = 0;
	s->ted_gain_q20 = 0;
	s->ted_sps = 0;
	s->ted_mu_q20 = 0;
	/* Squelch estimator init */
	s->squelch_running_power = 0;
	s->squelch_decim_stride = 16; /* evaluate 1/16th samples for low CPU */
	s->squelch_decim_phase = 0;
	s->squelch_window = 2048; /* EMA window ~2048 samples */
	/* HB decimator histories */
	for (int st = 0; st < 10; st++) {
		memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
		memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
	}
	/* Legacy CIC histories used by fifth_order path */
	for (int st = 0; st < 10; st++) {
		memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
		memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
	}
	/* Input ring does not require double-buffer init */
	s->lowpassed = s->input_cb_buf;
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	/* custom_atan mapping:
	   0 -> polar_discriminant (double atan2; slow, highest accuracy)
	   1 -> polar_disc_fast    (int64 fast_atan2 approximation)
	   2 -> polar_disc_lut     (LUT-based atan2 approximation)
	*/
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
	/* Init minimal worker pool (env-gated) */
	demod_mt_init(s);
}

/**
 * Initialize demodulator state for RO2 path (no CIC, LUT atan by default).
 *
 * @param s Demodulator state to initialize.
 */
void demod_init_ro2(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 2;
	s->deemph = 0;
	s->rate_out2 = rtl_bandwidth;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->deemph_avg = 0;
	/* Audio LPF defaults */
	s->audio_lpf_enable = 0;
	s->audio_lpf_alpha = 0;
	s->audio_lpf_state = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
	s->dc_avg = 0;
	/* Resampler defaults */
	s->resamp_enabled = 0;
	s->resamp_target_hz = 0;
	s->resamp_L = 1;
	s->resamp_M = 1;
	s->resamp_phase = 0;
	s->resamp_taps_len = 0;
	s->resamp_taps_per_phase = 0;
	s->resamp_taps = NULL;
	s->resamp_hist = NULL;
	/* FLL/TED defaults */
	s->fll_enabled = 0;
	s->fll_alpha_q15 = 0;
	s->fll_beta_q15 = 0;
	s->fll_freq_q15 = 0;
	s->fll_phase_q15 = 0;
	s->fll_prev_r = 0;
	s->fll_prev_j = 0;
	s->ted_enabled = 0;
	s->ted_gain_q20 = 0;
	s->ted_sps = 0;
	s->ted_mu_q20 = 0;
	/* Squelch estimator init */
	s->squelch_running_power = 0;
	s->squelch_decim_stride = 16;
	s->squelch_decim_phase = 0;
	s->squelch_window = 2048;
	/* HB decimator histories */
	for (int st = 0; st < 10; st++) {
		memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
		memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
	}
	/* Legacy CIC histories used by fifth_order path */
	for (int st = 0; st < 10; st++) {
		memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
		memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
	}
	/* Input ring does not require double-buffer init */
	s->lowpassed = s->input_cb_buf;
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
	/* Init minimal worker pool (env-gated) */
	demod_mt_init(s);
}

/**
 * Initialize demodulator state for default digital path.
 *
 * @param s Demodulator state to initialize.
 */
void demod_init(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 2;
	s->deemph = 0;
	s->rate_out2 = -1;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->deemph_avg = 0;
	/* Audio LPF defaults */
	s->audio_lpf_enable = 0;
	s->audio_lpf_alpha = 0;
	s->audio_lpf_state = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
	s->dc_avg = 0;
	/* Resampler defaults */
	s->resamp_enabled = 0;
	s->resamp_target_hz = 0;
	s->resamp_L = 1;
	s->resamp_M = 1;
	s->resamp_phase = 0;
	s->resamp_taps_len = 0;
	s->resamp_taps_per_phase = 0;
	s->resamp_taps = NULL;
	s->resamp_hist = NULL;
	/* FLL/TED defaults */
	s->fll_enabled = 0;
	s->fll_alpha_q15 = 0;
	s->fll_beta_q15 = 0;
	s->fll_freq_q15 = 0;
	s->fll_phase_q15 = 0;
	s->fll_prev_r = 0;
	s->fll_prev_j = 0;
	s->ted_enabled = 0;
	s->ted_gain_q20 = 0;
	s->ted_sps = 0;
	s->ted_mu_q20 = 0;
	/* Squelch estimator init */
	s->squelch_running_power = 0;
	s->squelch_decim_stride = 16;
	s->squelch_decim_phase = 0;
	s->squelch_window = 2048;
	/* HB decimator histories */
	for (int st = 0; st < 10; st++) {
		memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
		memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
	}
	/* Legacy CIC histories used by fifth_order path */
	for (int st = 0; st < 10; st++) {
		memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
		memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
	}
	/* Input ring does not require double-buffer init */
	s->lowpassed = s->input_cb_buf;
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
	/* Init minimal worker pool (env-gated) */
	demod_mt_init(s);
}

/**
 * Release resources owned by the demodulator state.
 *
 * @param s Demodulator state to clean up.
 */
void demod_cleanup(struct demod_state *s)
{
	pthread_cond_destroy(&s->ready);
	pthread_mutex_destroy(&s->ready_m);
	/* Destroy worker pool if enabled */
	demod_mt_destroy(s);
	/* Free resampler resources */
	if (s->resamp_taps) { free(s->resamp_taps); s->resamp_taps = NULL; }
	if (s->resamp_hist) { free(s->resamp_hist); s->resamp_hist = NULL; }
}

/**
 * Initialize output ring buffer and synchronization primitives.
 *
 * @param s Output state to initialize.
 */
void output_init(struct output_state *s)
{
	s->rate = rtl_bandwidth;
	pthread_cond_init(&s->ready, NULL);
	pthread_cond_init(&s->space, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	/* Allocate SPSC ring buffer */
	s->capacity = (size_t)(MAXIMUM_BUF_LENGTH * 8);
	/* Try aligned allocation for better vectorized copies; fall back if unavailable */
	{
		void *mem_ptr = NULL;
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
		if (posix_memalign(&mem_ptr, DSD_FME_ALIGN, s->capacity * sizeof(int16_t)) != 0) {
			mem_ptr = malloc(s->capacity * sizeof(int16_t));
		}
#else
		mem_ptr = malloc(s->capacity * sizeof(int16_t));
#endif
		s->buffer = static_cast<int16_t*>(mem_ptr);
	}
	s->head.store(0);
	s->tail.store(0);
}

/**
 * Destroy output ring buffer and synchronization primitives.
 *
 * @param s Output state to clean up.
 */
void output_cleanup(struct output_state *s)
{
	pthread_cond_destroy(&s->ready);
	pthread_cond_destroy(&s->space);
	pthread_mutex_destroy(&s->ready_m);
	if (s->buffer) { free(s->buffer); s->buffer = NULL; }
}

/**
 * Initialize controller state (frequency list and hop control).
 *
 * @param s Controller state to initialize.
 */
void controller_init(struct controller_state *s)
{
	s->freqs[0] = 446000000;
	s->freq_len = 0;
	s->edge = 0;
	s->wb_mode = 0;
	pthread_cond_init(&s->hop, NULL);
	pthread_mutex_init(&s->hop_m, NULL);
}

/**
 * Destroy controller synchronization primitives.
 *
 * @param s Controller state to clean up.
 */
void controller_cleanup(struct controller_state *s)
{
	pthread_cond_destroy(&s->hop);
	pthread_mutex_destroy(&s->hop_m);
}

/**
 * Validate runtime options and controller state prior to starting streams.
 * Exits the process with an error message if constraints are violated.
 */
void sanity_checks(void)
{
	if (controller.freq_len == 0) {
		fprintf (stderr, "Please specify a frequency.\n");
		exit(1);
	}

	if (controller.freq_len >= FREQUENCIES_LIMIT) {
		fprintf (stderr, "Too many channels, maximum %i.\n", FREQUENCIES_LIMIT);
		exit(1);
	}

	if (controller.freq_len > 1 && demod.squelch_level == 0) {
		fprintf (stderr, "Please specify a squelch level.  Required for scanning multiple frequencies.\n");
		exit(1);
	}

}

/**
 * Convert a 5-byte UDP control message into an integer.
 * Expects the first byte to be a command and the next four bytes to represent
 * a little-endian 32-bit value.
 *
 * @param buf Pointer to 5-byte buffer.
 * @return Decoded 32-bit little-endian integer from bytes 1..4.
 */
static unsigned int chars_to_int(unsigned char* buf) {

	int i;
	unsigned int val = 0;

	for(i=1; i<5; i++) {
		val = val | ((buf[i]) << ((i-1)*8));
	}

	return val;
}

/**
 * UDP control thread: listens for frequency tuning commands and applies them.
 *
 * Message format: 5 bytes, where buf[0]==0 indicates a set-frequency command
 * and buf[1..4] is a little-endian 32-bit frequency in Hz.
 *
 * @param arg Unused.
 * @return NULL on exit.
 */
static void *socket_thread_fn(void *arg) {
  UNUSED(arg);

  int n;
  int sockfd;
  unsigned char buffer[5];
  struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (sockfd < 0) {
  	perror("ERROR opening socket");
  }

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,  sizeof(serv_addr)) < 0) {
		perror("ERROR on binding");
	}

	bzero(buffer,5);

	fprintf (stderr, "Main socket started! :-) Tuning enabled on UDP/%d \n", port);

	int new_freq;

	while ((n = read(sockfd, buffer, 5)) > 0) {
		if (n == 5 && buffer[0] == 0) {
			new_freq = chars_to_int(buffer);
			dongle.freq = new_freq;
			optimal_settings(new_freq, demod.rate_in);
			rtlsdr_set_center_freq(dongle.dev, dongle.freq);
			fprintf (stderr, "\nTuning to: %d [Hz] \n", new_freq);
		}
	}
	if (n < 0) {
		perror("ERROR on read");
	}

	close(sockfd);
	return 0;
}

/**
 * Signal handler to request RTL-SDR async cancel and exit.
 */
void rtlsdr_sighandler(void)
{
	fprintf (stderr, "Signal caught, exiting!\n");
	rtlsdr_cancel_async(dongle.dev);
}

/**
 * Initialize and open the RTL-SDR streaming pipeline, threads, and buffers.
 *
 * @param opts Decoder options used to configure the pipeline.
 */
void open_rtlsdr_stream(dsd_opts *opts)
{
  int r;
	rtl_bandwidth =  opts->rtl_bandwidth * 1000; //reverted back to straight value
	bandwidth_multiplier = (bandwidth_divisor / rtl_bandwidth);
	/* Guard multiplier to a safe range [1, MAX_BANDWIDTH_MULTIPLIER] */
	{
		int orig_mult = bandwidth_multiplier;
		if (bandwidth_multiplier < 1) {
			fprintf(stderr,
				"WARNING: bandwidth_multiplier computed as %d (divisor=%d, bandwidth=%d Hz). Clamping to 1.\n",
				orig_mult, bandwidth_divisor, rtl_bandwidth);
			bandwidth_multiplier = 1;
		} else if (bandwidth_multiplier > MAX_BANDWIDTH_MULTIPLIER) {
			fprintf(stderr,
				"WARNING: bandwidth_multiplier computed as %d exceeds max %d (divisor=%d, bandwidth=%d Hz). Clamping to %d.\n",
				orig_mult, MAX_BANDWIDTH_MULTIPLIER, bandwidth_divisor, rtl_bandwidth, MAX_BANDWIDTH_MULTIPLIER);
			bandwidth_multiplier = MAX_BANDWIDTH_MULTIPLIER;
		}
	}
	volume_multiplier = 1; //moved to external handling to be more dynamic

	//this needs to be initted first, then we set the parameters
  dongle_init(&dongle);
	//init with low pass if decoding P25 or EDACS/Provoice
	if (opts->frame_p25p1 == 1 || opts->frame_p25p2 == 1 || opts->frame_provoice == 1)
  	demod_init_ro2(&demod);
	else if (opts->analog_only == 1 || opts->m17encoder == 1)
		demod_init_analog(&demod);
	else demod_init(&demod);
  output_init(&output);
  /* Init input ring */
  {
    void *mem_ptr = NULL;
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
    if (posix_memalign(&mem_ptr, DSD_FME_ALIGN, (size_t)(MAXIMUM_BUF_LENGTH * 8) * sizeof(int16_t)) != 0) {
      mem_ptr = malloc((size_t)(MAXIMUM_BUF_LENGTH * 8) * sizeof(int16_t));
    }
#else
    mem_ptr = malloc((size_t)(MAXIMUM_BUF_LENGTH * 8) * sizeof(int16_t));
#endif
    input_ring.buffer = static_cast<int16_t*>(mem_ptr);
    input_ring.capacity = (size_t)(MAXIMUM_BUF_LENGTH * 8);
    input_ring.head.store(0);
    input_ring.tail.store(0);
    pthread_cond_init(&input_ring.ready, NULL);
    pthread_mutex_init(&input_ring.ready_m, NULL);
  }
  controller_init(&controller);

	/* Read optional environment flags */
	{
		const char *hb = getenv("DSD_FME_HB_DECIM");
		if (hb && hb[0] != '\0') {
			int v = atoi(hb);
			use_halfband_decimator = (v != 0);
		}
		const char *cr = getenv("DSD_FME_COMBINE_ROT");
		if (cr && cr[0] != '\0') {
			combine_rotate_enabled = (atoi(cr) != 0);
		}
		const char *ufp = getenv("DSD_FME_UPSAMPLE_FP");
		if (ufp && ufp[0] != '\0') {
			upsample_fixedpoint_enabled = (atoi(ufp) != 0);
		}
		/* Configure rational resampler target rate via DSD_FME_RESAMP (Hz).
		   Defaults: enabled at 48000 Hz unless set to "off" or "0". */
		const char *rs = getenv("DSD_FME_RESAMP");
		int enable_resamp = 1;
		int target = 48000;
		if (rs && rs[0] != '\0') {
			if (strcasecmp(rs, "off") == 0 || strcmp(rs, "0") == 0) {
				enable_resamp = 0;
			} else {
				int v = atoi(rs);
				if (v > 0) target = v; else target = 48000;
			}
		}
		if (enable_resamp) {
			demod.resamp_target_hz = target;
			int inRate = (demod.rate_out > 0) ? demod.rate_out : rtl_bandwidth;
			int g = gcd_int(inRate, target);
			int L = target / g;
			int M = inRate / g;
			if (L < 1) L = 1;
			if (M < 1) M = 1;
			/* Guard output buffer growth (limited to ~4x expansion) */
			int scale_num = L;
			int scale_den = M;
			int scale = (scale_den > 0) ? ( (scale_num + scale_den - 1) / scale_den ) : 1;
			if (scale > 4) {
				fprintf(stderr, "Resampler ratio too large (L=%d,M=%d). Clamping not supported; disabling resampler.\n", L, M);
				demod.resamp_enabled = 0;
			} else {
				demod.resamp_enabled = 1;
				resamp_design(&demod, L, M);
				fprintf(stderr, "Rational resampler enabled: %d -> %d Hz (L=%d,M=%d).\n", inRate, target, L, M);
			}
		} else {
			demod.resamp_enabled = 0;
		}

		/* Configure FLL/TED via envs. Defaults: FLL on with small gains; TED off by default. */
		const char *fll = getenv("DSD_FME_FLL");
		demod.fll_enabled = (!fll || fll[0] == '\0' || fll[0] == '1') ? 1 : 0;
		/* Optional: enable LUT-based FLL rotator via DSD_FME_FLL_LUT=1 */
		{
			const char *flut = getenv("DSD_FME_FLL_LUT");
			fll_lut_enabled = (flut && flut[0] == '1') ? 1 : 0;
		}
		/* Gains in Q15; very conservative defaults */
		const char *fa = getenv("DSD_FME_FLL_ALPHA");
		const char *fb = getenv("DSD_FME_FLL_BETA");
		demod.fll_alpha_q15 = fa ? atoi(fa) : 100;  /* ~0.003 */
		demod.fll_beta_q15  = fb ? atoi(fb) : 10;   /* ~0.0003 */
		demod.fll_freq_q15  = 0;
		demod.fll_phase_q15 = 0;
		demod.fll_prev_r = demod.fll_prev_j = 0;

		const char *ted = getenv("DSD_FME_TED");
		demod.ted_enabled = (ted && ted[0] == '1') ? 1 : 0;
		const char *tg = getenv("DSD_FME_TED_GAIN");
		const char *ts = getenv("DSD_FME_TED_SPS");
		const char *tf = getenv("DSD_FME_TED_FORCE");
		demod.ted_gain_q20 = tg ? atoi(tg) : 64; /* tiny default */
		demod.ted_sps = ts ? atoi(ts) : 10;      /* e.g., 4800 sym/s @ 48k */
		demod.ted_mu_q20 = 0;
		demod.ted_force = (tf && tf[0] == '1') ? 1 : 0;

		/* Mode-aware defaults (only if envs not provided) */
		int env_ted_set = (ted && ted[0] != '\0');
		int env_fll_alpha_set = (fa && fa[0] != '\0');
		int env_fll_beta_set  = (fb && fb[0] != '\0');
		int env_ted_sps_set   = (ts && ts[0] != '\0');
		int env_ted_gain_set  = (tg && tg[0] != '\0');
		int digital_mode = (opts->frame_p25p1 == 1 || opts->frame_p25p2 == 1 || opts->frame_provoice == 1);
		/* Default: for common digital modes compute reasonable defaults, but keep TED disabled unless user opts in. */
		if (digital_mode) {
			if (!env_ted_set) demod.ted_enabled = 0;
			if (!env_ted_sps_set) {
				int Fs = (int)output.rate;
				int sps = (Fs + 2400) / 4800; /* round(Fs/4800) */
				if (sps < 2) sps = 2;
				demod.ted_sps = sps;
			}
			if (!env_ted_gain_set) {
				/* Slightly higher but safe default gain for digital */
				demod.ted_gain_q20 = 96;
			}
			if (!env_fll_alpha_set) demod.fll_alpha_q15 = 150; /* ~0.0046 */
			if (!env_fll_beta_set)  demod.fll_beta_q15  = 15;  /* ~0.00046 */
		} else {
			/* Analog defaults: keep TED off; gentle FLL */
			if (!env_ted_set) demod.ted_enabled = 0;
			if (!env_fll_alpha_set) demod.fll_alpha_q15 = 50; /* ~0.0015 */
			if (!env_fll_beta_set)  demod.fll_beta_q15  = 5;  /* ~0.00015 */
		}
	}

	if (opts->rtlsdr_center_freq > 0) {
		controller.freqs[controller.freq_len] = opts->rtlsdr_center_freq;
		controller.freq_len++;
	}

	if (opts->rtlsdr_ppm_error != 0) {
		dongle.ppm_error = opts->rtlsdr_ppm_error;
		fprintf (stderr, "Setting RTL PPM Error Set to %d\n", opts->rtlsdr_ppm_error);
	}

	dongle.dev_index = opts->rtl_dev_index;
	// demod.squelch_level = opts->rtl_squelch_level; //no longer used here, used in framesync vc pwr value under select conditions
	fprintf (stderr, "Setting RTL Bandwidth to %d Hz\n", rtl_bandwidth);
	// fprintf (stderr, "Setting RTL Sample Multiplier to %d\n", bandwidth_multiplier);
	fprintf (stderr, "Setting RTL Power Squelch Level to %d\n", opts->rtl_squelch_level);
	if (opts->rtl_udp_port != 0) port = opts->rtl_udp_port; //set this here, only open socket thread if set
	if (opts->rtl_gain_value > 0) {
		dongle.gain = opts->rtl_gain_value * 10; //multiple by ten to make it consitent with the way rtl_fm works
	}

  /* quadruple sample_rate to limit to Δθ to ±π/2 */
	demod.rate_in *= demod.post_downsample;

  if (!output.rate) output.rate = demod.rate_out;

  sanity_checks();

	if (controller.freq_len > 1) demod.terminate_on_squelch = 0;

  ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;
  /* Ensure async read uses a valid, explicit buffer length */
  dongle.buf_len = (uint32_t)ACTUAL_BUF_LENGTH;

  r = rtlsdr_open(&dongle.dev, (uint32_t)dongle.dev_index);
  if (r < 0)
  {
    fprintf (stderr, "Failed to open rtlsdr device %d.\n", dongle.dev_index);
    exit(1);
  } else {
		fprintf (stderr, "Using RTLSDR Device Index: %d. \n", dongle.dev_index);
	}

  if (demod.deemph) {
		/* Configure deemphasis via env DSD_FME_DEEMPH: 75 (default), 50, nfm, off.
		   Computes a one-pole IIR with alpha = 1 - exp(-1/(Fs*tau)) stored in Q15. */
		double tau_s = 75e-6; /* default 75 microseconds */
		const char *deemph_env = getenv("DSD_FME_DEEMPH");
		if (deemph_env && deemph_env[0] != '\0') {
			if (strcasecmp(deemph_env, "off") == 0) {
				demod.deemph = 0;
			} else if (strcmp(deemph_env, "50") == 0) {
				tau_s = 50e-6;
			} else if (strcasecmp(deemph_env, "nfm") == 0) {
				/* Common NFM value */
				tau_s = 750e-6;
			} else if (strcmp(deemph_env, "75") == 0) {
				tau_s = 75e-6;
			}
		}
		if (demod.deemph) {
			/* a = exp(-1/(Fs*tau)); store alpha=(1-a) in Q15 */
			double Fs = (double)demod.rate_out;
			if (Fs < 1.0) Fs = 1.0;
			double a = exp(-1.0 / (Fs * tau_s));
			double alpha = 1.0 - a;
			int coef_q15 = (int)lrint(alpha * (double)(1 << 15));
			if (coef_q15 < 1) coef_q15 = 1; /* ensure non-zero to move toward steady-state */
			if (coef_q15 > (1 << 15)) coef_q15 = (1 << 15);
			demod.deemph_a = coef_q15;
		}
	}

	/* Configure optional post-demod audio LPF via env DSD_FME_AUDIO_LPF.
	   Values:
	   - off or 0: disabled (default)
	   - NNNN: cutoff in Hz (approximate), e.g., 3000 or 5000.
	   One-pole: y[n] = y[n-1] + alpha * (x[n] - y[n-1]),
	   alpha ≈ 1 - exp(-2*pi*fc/Fs) in Q15. */
	{
		const char *alpf = getenv("DSD_FME_AUDIO_LPF");
		demod.audio_lpf_enable = 0;
		demod.audio_lpf_alpha = 0;
		demod.audio_lpf_state = 0;
		if (alpf && alpf[0] != '\0') {
			if (strcasecmp(alpf, "off") == 0 || strcmp(alpf, "0") == 0) {
				/* disabled */
			} else {
				int cutoff_hz = atoi(alpf);
				if (cutoff_hz < 100) cutoff_hz = 100; /* guard */
				/* One-pole mapping: choose alpha from cutoff and Fs using approx alpha = 1 - exp(-2*pi*fc/Fs) */
				double Fs = (double)demod.rate_out;
				if (Fs < 1.0) Fs = 1.0;
				double a = 1.0 - exp(-2.0 * kPi * (double)cutoff_hz / Fs);
				if (a < 0.0) a = 0.0;
				if (a > 1.0) a = 1.0;
				int alpha_q15 = (int)lrint(a * (double)(1 << 15));
				if (alpha_q15 < 1) alpha_q15 = 1;
				if (alpha_q15 > (1 << 15)) alpha_q15 = (1 << 15);
				demod.audio_lpf_alpha = alpha_q15;
				demod.audio_lpf_enable = 1;
				fprintf(stderr, "Audio LPF enabled: fc≈%d Hz, alpha_q15=%d\n", cutoff_hz, demod.audio_lpf_alpha);
			}
		}
	}

  /* Set the tuner gain */
	if (dongle.gain == AUTO_GAIN) {
		verbose_auto_gain(dongle.dev);
		fprintf (stderr, "Setting RTL Autogain. \n");
	} else {
		dongle.gain = nearest_gain(dongle.dev, dongle.gain);
		verbose_gain_set(dongle.dev, dongle.gain);
		// fprintf (stderr, "Setting RTL Nearest Gain to %d. \n", dongle.gain); //seems to be working now
	}

  verbose_ppm_set(dongle.dev, dongle.ppm_error);

  /* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dongle.dev);

  pthread_create(&controller.thread, NULL, controller_thread_fn, (void*)(&controller));
  usleep(100000);
  pthread_create(&demod.thread, NULL, demod_thread_fn, (void*)(&demod));
  pthread_create(&dongle.thread, NULL, dongle_thread_fn, (void*)(&dongle));
	//only create socket thread IF user specified (for legacy uses), else don't use it
	if (port != 0) pthread_create(&socket_freq, NULL, socket_thread_fn, (void *)(&controller));

	/* If resampler is enabled, update output.rate for downstream consumers */
	if (demod.resamp_enabled && demod.resamp_target_hz > 0) {
		output.rate = demod.resamp_target_hz;
		fprintf(stderr, "Output rate set to %d Hz via resampler.\n", output.rate);
	} else {
		output.rate = demod.rate_out;
	}
}

/**
 * Stop threads, cleanup buffers/objects, and close the RTL-SDR stream.
 */
void cleanup_rtlsdr_stream(void)
{
	fprintf (stderr, "cleaning up...\n");
  rtlsdr_cancel_async(dongle.dev);
  pthread_join(dongle.thread, NULL);
  safe_cond_signal(&demod.ready, &demod.ready_m);
  pthread_join(demod.thread, NULL);
  safe_cond_signal(&output.ready, &output.ready_m);
  safe_cond_signal(&controller.hop, &controller.hop_m);
  pthread_join(controller.thread, NULL);

  //dongle_cleanup(&dongle);
  demod_cleanup(&demod);
  output_cleanup(&output);
  controller_cleanup(&controller);

	/* free input ring */
	if (input_ring.buffer) { free(input_ring.buffer); input_ring.buffer = NULL; }

	/* free LUT memory if allocated */
	atan_lut_free();

  rtlsdr_close(dongle.dev);
}

/**
 * Batched consumer API: read up to count samples with fewer wakeups/locks.
 * Applies volume scaling.
 *
 * @param out   Destination buffer for audio samples.
 * @param count Maximum number of samples to read.
 * @param opts  Decoder options (used for runtime PPM changes).
 * @param state Decoder state (unused).
 * @return Number of samples read (>=1) or -1 on exit.
 */
int get_rtlsdr_samples(int16_t *out, size_t count, dsd_opts * opts, dsd_state * state)
{
	UNUSED(state);
	if (count == 0) return 0;

	/* If PPM Error is Manually Changed, change it here once per batch */
	if (opts->rtlsdr_ppm_error != dongle.ppm_error)
	{
		dongle.ppm_error = opts->rtlsdr_ppm_error;
		verbose_ppm_set(dongle.dev, dongle.ppm_error);
	}

	int got = ring_read_batch(&output, out, count);
	if (got <= 0) {
		return -1;
	}
	/* Apply volume scaling with saturation */
	for (int i = 0; i < got; i++) {
		int32_t y = (int32_t)out[i] * (int32_t)volume_multiplier;
		out[i] = sat16(y);
	}
	return got;
}

/**
 * Convenience wrapper to read a single sample via the batched API.
 *
 * @param sample Destination for one sample.
 * @param opts   Decoder options.
 * @param state  Decoder state (unused).
 * @return 0 on success, -1 on exit.
 */
int get_rtlsdr_sample(int16_t *sample, dsd_opts * opts, dsd_state * state)
{
	/* Delegate to batched API for a single sample */
	int ret = get_rtlsdr_samples(sample, 1, opts, state);
	if (ret < 0) return -1;
	return 0;
}

/**
 * Tune RTL-SDR to a new center frequency, updating optimal settings.
 *
 * @param opts      Decoder options.
 * @param frequency Target center frequency in Hz.
 */
void rtl_dev_tune(dsd_opts * opts, long int frequency)
{
	int r;
	if (opts->payload == 1)
		fprintf (stderr, "\nTuning to %lu Hz.", frequency);
	dongle.freq = opts->rtlsdr_center_freq = frequency;
	optimal_settings(dongle.freq, demod.rate_in);
	if (opts->payload == 1)
		fprintf (stderr, " (Center Frequency: %u Hz.) \n", dongle.freq);
	r = rtlsdr_set_center_freq(dongle.dev, dongle.freq);
	if (r < 0)
		fprintf (stderr, " (WARNING: Failed to set Center Frequency %u). \n", dongle.freq);

	rtl_clean_queue();

}

/**
 * Return mean power approximation (RMS^2 proxy) for soft squelch decisions.
 * Uses a small fixed sample window for efficiency.
 *
 * @return Mean power value (approximate RMS squared).
 */
long int rtl_return_pwr(void)
{
	long int pwr = 0;
	int n = demod.lp_len;
	if (n > 160) n = 160;
	if (n < 0) n = 0;
	pwr = mean_power(demod.lowpassed, n, 1);
	return (pwr);
}

/**
 * Clear the output ring buffer and wake any waiting producer.
 */
void rtl_clean_queue(void)
{
	/* Clear the entire ring to prevent sample 'lag' */
	ring_clear(&output);
	/* Wake producer waiting for space */
	safe_cond_signal(&output.space, &output.ready_m);
}