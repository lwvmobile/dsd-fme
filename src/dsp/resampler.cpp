/*
 * Rational Resampler Implementation
 *
 * This file implements the polyphase rational resampler for sample rate
 * conversion using L/M filtering. It provides high-quality audio resampling
 * with efficient polyphase filter implementation for real-time operation.
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

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dsp/fll.h"
#include "dsp/resampler.h"
#include "dsp/ted.h"

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

/*
 * Local definition of demod_state structure for this module.
 * MUST MATCH the layout in src/rtl_sdr_fm.cpp at least up through the fields
 * accessed here (including all preceding fields). Keep in sync with
 * src/dsp/demod_pipeline.cpp to ensure offset compatibility.
 */

#define MAXIMUM_OVERSAMPLE       16
#define DEFAULT_BUF_LENGTH       16384
#define MAXIMUM_BUF_LENGTH       (MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define MAX_BANDWIDTH_MULTIPLIER 8

struct output_state; /* forward */

struct demod_state {
    int exit_flag;
    pthread_t thread;
    int16_t* lowpassed;
    alignas(64) int16_t input_cb_buf[MAXIMUM_BUF_LENGTH];
    int lp_len;
    int16_t lp_i_hist[10][6];
    int16_t lp_q_hist[10][6];
    alignas(64) int16_t result[MAXIMUM_BUF_LENGTH];
    int16_t droop_i_hist[9];
    int16_t droop_q_hist[9];
    int result_len;
    int rate_in;
    int rate_out;
    int rate_out2;
    int now_r, now_j;
    int pre_r, pre_j;
    int prev_index;
    int downsample; /* min 1, max 256 */
    int post_downsample;
    int output_scale;
    int squelch_level, conseq_squelch, squelch_hits, terminate_on_squelch;
    int64_t squelch_running_power;
    int squelch_decim_stride;
    int squelch_decim_phase;
    int squelch_window;
    int downsample_passes;
    int comp_fir_size;
    int custom_atan;
    int deemph, deemph_a;
    int deemph_avg;
    int audio_lpf_enable;
    int audio_lpf_alpha;
    int audio_lpf_state;
    int now_lpr;
    int prev_lpr_index;
    int dc_block, dc_avg;
    int16_t hb_workbuf[MAXIMUM_BUF_LENGTH];
    int16_t hb_hist_i[10][14];
    int16_t hb_hist_q[10][14];
    alignas(64) int16_t hb_i_buf[MAXIMUM_BUF_LENGTH / 2];
    alignas(64) int16_t hb_q_buf[MAXIMUM_BUF_LENGTH / 2];
    alignas(64) int16_t hb_i_out[MAXIMUM_BUF_LENGTH / 2];
    alignas(64) int16_t hb_q_out[MAXIMUM_BUF_LENGTH / 2];
    alignas(64) int16_t upsample_buf[MAXIMUM_BUF_LENGTH * MAX_BANDWIDTH_MULTIPLIER];
    /* Polyphase rational resampler (L/M) state and output buffer */
    int resamp_enabled;
    int resamp_target_hz;      /* desired output sample rate */
    int resamp_L;              /* upsample factor */
    int resamp_M;              /* downsample factor */
    int resamp_phase;          /* 0..L-1 accumulator */
    int resamp_taps_len;       /* prototype taps length (padded to K*L) */
    int resamp_taps_per_phase; /* K = ceil(taps_len/L) */
    int16_t* resamp_taps;      /* Q15 taps, length = K*L */
    int16_t* resamp_hist;      /* circular history, length = K */
    int resamp_hist_head;      /* head index into circular history [0..K-1] */
    alignas(64) int16_t resamp_outbuf[MAXIMUM_BUF_LENGTH * 4];
    /* FLL/TED state (not used here, kept for layout compatibility) */
    int fll_enabled;
    int fll_alpha_q15;
    int fll_beta_q15;
    int fll_freq_q15;
    int fll_phase_q15;
    int fll_deadband_q14;
    int fll_slew_max_q15;
    int fll_prev_r;
    int fll_prev_j;
    int ted_enabled;
    int ted_force;
    int ted_gain_q20;
    int ted_sps;
    int ted_mu_q20;
    alignas(64) int16_t timing_buf[MAXIMUM_BUF_LENGTH];
    fll_state_t fll_state;
    ted_state_t ted_state;
    int mt_enabled;
    int mt_ready;
    pthread_t mt_threads[2];
    pthread_mutex_t mt_lock;
    pthread_cond_t mt_cv;
    pthread_cond_t mt_done_cv;
    int mt_should_exit;
    int mt_epoch;
    int mt_completed_in_epoch;
    int mt_posted_count;

    struct {
        void (*run)(void*);
        void* arg;
    } mt_tasks[2];

    int mt_worker_id[2];

    struct {
        struct demod_state* s;
        int id;
    } mt_args[2];

    int (*discriminator)(int, int, int, int);
    void (*mode_demod)(struct demod_state*);
    pthread_cond_t ready;
    pthread_mutex_t ready_m;
    struct output_state* output_target;
};

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
 * Design windowed-sinc low-pass prototype for polyphase upfirdn (runs at L*Fs_in).
 * Taps are stored phase-major with stride L (k*L + phase). The function allocates
 * aligned storage for taps and history inside the provided demod_state and
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
 * Process one block using polyphase upfirdn with history.
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
