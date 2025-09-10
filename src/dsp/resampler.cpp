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
 * @brief Polyphase rational resampler implementation (L/M) with SIMD optimizations.
 *
 * Implements design and block processing for a windowed-sinc upfirdn resampler
 * used by the FM audio path. Includes portable scalar code and optional SSE2/NEON
 * vectorized inner products for the common 16-tap-per-phase configuration.
 */

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dsp/demod_state.h"
#include "dsp/resampler.h"

/* We include the demod state definition from the compilation unit that
 * declares it. Here we forward-declare only; fields are accessed via s->.
 * The concrete struct is defined in rtl_sdr_fm.cpp. */

/* Alignment assumptions mirror those used in rtl_sdr_fm.cpp */
#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_PRAGMA(x) _Pragma(#x)
#define DSD_FME_IVDEP     DSD_FME_PRAGMA(GCC ivdep)
#else
#define DSD_FME_IVDEP
#endif

#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif
#include "runtime/mem.h"

#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_RESTRICT __restrict__
#else
#define DSD_FME_RESTRICT
#endif

/* kPi constant and helpers kept local */
static const double kPi = 3.14159265358979323846;

template <typename T>
static inline T*
assume_aligned_ptr(T* p, size_t /*align_unused*/) {
#if defined(__GNUC__) || defined(__clang__)
    return (T*)__builtin_assume_aligned(p, 64);
#else
    return p;
#endif
}

template <typename T>
static inline const T*
assume_aligned_ptr(const T* p, size_t /*align_unused*/) {
#if defined(__GNUC__) || defined(__clang__)
    return (const T*)__builtin_assume_aligned(p, 64);
#else
    return p;
#endif
}

/* demod_state now provided by include/dsp/demod_state.h */

static inline double
dsd_fme_sinc(double x) {
    if (x == 0.0) {
        return 1.0;
    }
    return sin(kPi * x) / (kPi * x);
}

static inline int16_t
sat16_local(int32_t x) {
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

/**
 * @brief Design windowed-sinc low-pass prototype for polyphase upfirdn (runs at L*Fs_in).
 *
 * Taps are stored phase-major with stride L (k*L + phase). The function allocates
 * aligned storage for taps and history inside the provided `demod_state` and
 * initializes the resampler bookkeeping fields.
 *
 * @param s Demodulator state to receive resampler taps/history.
 * @param L Upsampling factor.
 * @param M Downsampling factor.
 */
void
resamp_design(struct demod_state* s, int L, int M) {
    int taps_per_phase = 16; /* K */
    int total_taps = taps_per_phase * L;
    if (total_taps < L) {
        total_taps = L;
    }
    if (taps_per_phase < 8) {
        taps_per_phase = 8;
    }

    double fc = 0.45 / (double)((L > M) ? L : M);
    int N = total_taps;
    int mid = (N - 1) / 2;

    if (s->resamp_taps) {
        dsd_fme_aligned_free(s->resamp_taps);
        s->resamp_taps = NULL;
    }
    if (s->resamp_hist) {
        dsd_fme_aligned_free(s->resamp_hist);
        s->resamp_hist = NULL;
    }
    {
        void* mem_ptr = dsd_fme_aligned_malloc((size_t)N * sizeof(int16_t));
        s->resamp_taps = (int16_t*)mem_ptr;
    }
    {
        void* mem_ptr = dsd_fme_aligned_malloc((size_t)taps_per_phase * sizeof(int16_t));
        s->resamp_hist = (int16_t*)mem_ptr;
    }
    if (!s->resamp_taps || !s->resamp_hist) {
        if (s->resamp_taps) {
            free(s->resamp_taps);
            s->resamp_taps = NULL;
        }
        if (s->resamp_hist) {
            free(s->resamp_hist);
            s->resamp_hist = NULL;
        }
        s->resamp_enabled = 0;
        return;
    }
    memset(s->resamp_hist, 0, (size_t)taps_per_phase * sizeof(int16_t));
    s->resamp_hist_head = 0;

    double gain = 0.0;
    for (int n = 0; n < N; n++) {
        int m = n - mid;
        double w = 0.54 - 0.46 * cos(2.0 * kPi * (double)n / (double)(N - 1));
        double h = 2.0 * fc * dsd_fme_sinc(2.0 * fc * (double)m);
        double t = h * w;
        gain += t;
    }
    if (gain == 0.0) {
        gain = 1.0;
    }
    const double phase_gain_comp = (double)L;
    for (int n = 0; n < N; n++) {
        int m = n - mid;
        double w = 0.54 - 0.46 * cos(2.0 * kPi * (double)n / (double)(N - 1));
        double h = 2.0 * fc * dsd_fme_sinc(2.0 * fc * (double)m);
        double t = (h * w / gain) * phase_gain_comp;
        int v = (int)lrint(t * (double)(1 << 15));
        if (v > 32767) {
            v = 32767;
        }
        if (v < -32768) {
            v = -32768;
        }
        s->resamp_taps[n] = (int16_t)v;
    }

    s->resamp_L = L;
    s->resamp_M = M;
    s->resamp_phase = 0;
    s->resamp_taps_len = N;
    s->resamp_taps_per_phase = taps_per_phase;
}

static inline int64_t
dsd_fme_dot16_scalar(const int16_t* a, const int16_t* b) {
    int64_t acc = 0;
    for (int i = 0; i < 16; i++) {
        acc += (int32_t)a[i] * (int32_t)b[i];
    }
    return acc;
}

#if defined(__x86_64__) || defined(__i386__)
#if defined(__SSE2__)
#include <emmintrin.h>

static inline int64_t
dsd_fme_dot16_sse2(const int16_t* a, const int16_t* b) {
    __m128i va0 = _mm_loadu_si128((const __m128i*)a);
    __m128i vb0 = _mm_loadu_si128((const __m128i*)b);
    __m128i va1 = _mm_loadu_si128((const __m128i*)(a + 8));
    __m128i vb1 = _mm_loadu_si128((const __m128i*)(b + 8));
    __m128i p0 = _mm_madd_epi16(va0, vb0);
    __m128i p1 = _mm_madd_epi16(va1, vb1);
    int32_t t0[4], t1[4];
    _mm_storeu_si128((__m128i*)t0, p0);
    _mm_storeu_si128((__m128i*)t1, p1);
    int64_t acc = 0;
    acc += (int64_t)t0[0] + t0[1] + t0[2] + t0[3];
    acc += (int64_t)t1[0] + t1[1] + t1[2] + t1[3];
    return acc;
}
#endif
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>

static inline int64_t
dsd_fme_dot16_neon(const int16_t* a, const int16_t* b) {
    int16x8_t a0 = vld1q_s16(a);
    int16x8_t b0 = vld1q_s16(b);
    int16x8_t a1 = vld1q_s16(a + 8);
    int16x8_t b1 = vld1q_s16(b + 8);
    int32x4_t p0l = vmull_s16(vget_low_s16(a0), vget_low_s16(b0));
    int32x4_t p0h = vmull_s16(vget_high_s16(a0), vget_high_s16(b0));
    int32x4_t p1l = vmull_s16(vget_low_s16(a1), vget_low_s16(b1));
    int32x4_t p1h = vmull_s16(vget_high_s16(a1), vget_high_s16(b1));
    int32_t t0[4], t1[4], t2[4], t3[4];
    vst1q_s32(t0, p0l);
    vst1q_s32(t1, p0h);
    vst1q_s32(t2, p1l);
    vst1q_s32(t3, p1h);
    int64_t acc = 0;
    acc += (int64_t)t0[0] + t0[1] + t0[2] + t0[3];
    acc += (int64_t)t1[0] + t1[1] + t1[2] + t1[3];
    acc += (int64_t)t2[0] + t2[1] + t2[2] + t2[3];
    acc += (int64_t)t3[0] + t3[1] + t3[2] + t3[3];
    return acc;
}
#endif

/**
 * @brief Process one block using polyphase upfirdn with history.
 *
 * @param s      Demodulator state containing resampler state.
 * @param in     Pointer to input samples.
 * @param in_len Number of input samples.
 * @param out    Pointer to output buffer (sized to hold produced samples).
 * @return Number of output samples written.
 */
int
resamp_process_block(struct demod_state* s, const int16_t* DSD_FME_RESTRICT in, int in_len,
                     int16_t* DSD_FME_RESTRICT out) {
    if (!s->resamp_enabled || !s->resamp_taps || !s->resamp_hist) {
        memcpy(out, in, (size_t)in_len * sizeof(int16_t));
        return in_len;
    }
    const int L = s->resamp_L;
    const int M = s->resamp_M;
    const int K = s->resamp_taps_per_phase;
    const int16_t* DSD_FME_RESTRICT taps_al = assume_aligned_ptr(s->resamp_taps, DSD_FME_ALIGN);
    int phase = s->resamp_phase;
    int head = s->resamp_hist_head;
    int out_len = 0;
    const int16_t* DSD_FME_RESTRICT in_al = assume_aligned_ptr(in, DSD_FME_ALIGN);
    int16_t* DSD_FME_RESTRICT out_al = assume_aligned_ptr(out, DSD_FME_ALIGN);
    int16_t* DSD_FME_RESTRICT hist = assume_aligned_ptr(s->resamp_hist, DSD_FME_ALIGN);
    const int stride = L;
    const int use_mask = (K & (K - 1)) == 0;
    const int mask = K - 1;

    for (int n = 0; n < in_len; n++) {
        hist[head] = in_al[n];
        head++;
        if (head == K) {
            head = 0;
        }
        int local_phase = phase;
        while (local_phase < L) {
            int64_t acc = 0;
            const int16_t* DSD_FME_RESTRICT tk = taps_al + local_phase;
            if (K == 16) {
                int16_t hblk[16];
                int16_t tblk[16];
                if (use_mask) {
                    int idx = (head - 1) & mask;
                    for (int k = 0; k < 16; k++) {
                        hblk[k] = hist[idx];
                        tblk[k] = tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                    }
                } else {
                    int idx = head - 1;
                    if (idx < 0) {
                        idx += K;
                    }
                    for (int k = 0; k < 16; k++) {
                        hblk[k] = hist[idx];
                        tblk[k] = tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                    }
                }
#if defined(__x86_64__)
#if defined(__SSE2__)
                acc = dsd_fme_dot16_sse2(hblk, tblk);
#else
                acc = dsd_fme_dot16_scalar(hblk, tblk);
#endif
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
                acc = dsd_fme_dot16_neon(hblk, tblk);
#else
                acc = dsd_fme_dot16_scalar(hblk, tblk);
#endif
            } else {
                if (use_mask) {
                    int idx = (head - 1) & mask;
                    int k = 0;
                    for (; k + 3 < K; k += 4) {
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                    }
                    for (; k < K; k++) {
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx = (idx - 1) & mask;
                    }
                } else {
                    int idx = head - 1;
                    if (idx < 0) {
                        idx += K;
                    }
                    int k = 0;
                    for (; k + 3 < K; k += 4) {
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                    }
                    for (; k < K; k++) {
                        acc += (int32_t)hist[idx] * (int32_t)tk[0];
                        tk += stride;
                        idx--;
                        if (idx < 0) {
                            idx += K;
                        }
                    }
                }
            }
            acc += (1 << 14);
            int32_t y = (int32_t)(acc >> 15);
            out_al[out_len++] = sat16_local(y);
            local_phase += M;
        }
        phase = local_phase - L;
    }

    s->resamp_phase = phase;
    s->resamp_hist_head = head;
    return out_len;
}
