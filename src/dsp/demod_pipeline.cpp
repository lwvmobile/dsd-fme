#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "dsd.h"
#include "dsp/demod_pipeline.h"
#include "dsp/simd_widen.h"
#include "dsp/fll.h"
#include "dsp/ted.h"

/* Local definition of demod_state structure for this module */
struct demod_state {
    int exit_flag;
    pthread_t thread;
    int16_t* lowpassed;
    /* Scratch buffer for demod thread to read blocks from the input ring */
    /* Not a ring; callback writes directly into the global input ring. */
    alignas(64) int16_t input_cb_buf[262144];
    int lp_len;
    int16_t lp_i_hist[10][6];
    int16_t lp_q_hist[10][6];
    alignas(64) int16_t result[262144];
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
    /* Incremental, decimated RMS squelch estimator (power-domain, sqrt-free) */
    int64_t squelch_running_power;
    int squelch_decim_stride;
    int squelch_decim_phase;
    int squelch_window;
    int downsample_passes;
    int comp_fir_size;
    int custom_atan;
    int deemph, deemph_a;
    int deemph_avg;
    /* Optional post-demod audio low-pass filter (one-pole) */
    int audio_lpf_enable;
    int audio_lpf_alpha; /* Q15 alpha for one-pole LPF */
    int audio_lpf_state; /* state/output y[n-1] in Q0 */
    int now_lpr;
    int prev_lpr_index;
    int dc_block, dc_avg;
    /* Half-band decimator state */
    int16_t hb_workbuf[262144];
    int16_t hb_hist_i[10][14];  /* HB_TAPS-1 = 15-1 = 14 */
    int16_t hb_hist_q[10][14];
    /* Reserved buffers for potential deinterleave path (currently unused) */
    alignas(64) int16_t hb_i_buf[131072];
    alignas(64) int16_t hb_q_buf[131072];
    alignas(64) int16_t hb_i_out[131072];
    alignas(64) int16_t hb_q_out[131072];
    /* Preallocated buffer for linear upsampler (bandwidth_multiplier) */
    alignas(64) int16_t upsample_buf[262144 * 8];
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
    /* Output buffer for resampler (worst-case 4x expansion) */
    alignas(64) int16_t resamp_outbuf[262144 * 4];
    /* Residual CFO loop (FLL) state */
    int fll_enabled;
    int fll_alpha_q15;    /* proportional gain (Q15) */
    int fll_beta_q15;     /* integral gain (Q15) */
    int fll_freq_q15;     /* NCO frequency increment (Q15 radians/sample scaled) */
    int fll_phase_q15;    /* NCO phase accumulator (wrap at 2*pi -> 1<<15 scale) */
    int fll_deadband_q14; /* ignore small phase errors |err| <= deadband (Q14) */
    int fll_slew_max_q15; /* max |delta freq| per update (Q15) */
    int fll_prev_r;
    int fll_prev_j;
    /* Timing error detector (Gardner) fractional-delay state */
    int ted_enabled;
    int ted_force;    /* allow forcing TED even for FM/C4FM paths */
    int ted_gain_q20; /* small gain (Q20) for stability */
    int ted_sps;      /* nominal samples per symbol (e.g., 10 for 4800 sym/s at 48k) */
    int ted_mu_q20;   /* fractional phase [0,1) in Q20 */
    /* Work buffer for timing-adjusted I/Q */
    alignas(64) int16_t timing_buf[262144];
    /* FLL and TED module states (must match rtl_sdr_fm.cpp layout) */
    fll_state_t fll_state;
    ted_state_t ted_state;
    /* Minimal 2-thread worker pool for intra-block parallelism */
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
    /* Ready/condvar kept for cleanup compatibility; input ring is a global SPSC ring */
    pthread_cond_t ready; /* kept for cleanup compatibility; unused now */
    pthread_mutex_t ready_m;
    struct output_state* output_target;
};

/* Macros and constants from the original file */
#define MAXIMUM_OVERSAMPLE 16
#define DEFAULT_BUF_LENGTH 16384
#define MAXIMUM_BUF_LENGTH (MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define MAX_BANDWIDTH_MULTIPLIER 8

#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

#ifndef DSD_FME_RESTRICT
#define DSD_FME_RESTRICT __restrict__
#endif

#ifndef DSD_FME_IVDEP
#define DSD_FME_IVDEP
#endif

/* Platform-specific aligned pointer assumption */
template<typename T>
static inline T* assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return p;
}

template<typename T>
static inline const T* assume_aligned_ptr(const T* p, size_t /*align_unused*/) {
    return p;
}

/* Saturation helper for int16 */
static inline int16_t sat16(int32_t x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

/* Half-band decimator constants and tables */
#define HB_TAPS 15
#define HB_HALF ((HB_TAPS - 1) / 2)
static const int16_t hb_q15_taps[HB_TAPS] = {-108, 0, 1800, 0, -500, 0, 7000, 16384, 7000, 0, -500, 0, 1800, 0, -108};

/* CIC compensation filter tables */
#define CIC_TABLE_MAX 10
static const int cic_9_tables[][10] = {
    /* ds_p=0: no compensation needed */
    {0},
    /* ds_p=1: single stage */
    {0, 8192, 0, 0, 0, 0, 0, 0, 0, 0},
    /* ds_p=2: two stages */
    {0, 4096, 4096, 0, 0, 0, 0, 0, 0, 0},
    /* ds_p=3: three stages */
    {0, 2730, 2730, 2730, 0, 0, 0, 0, 0, 0},
    /* ds_p=4: four stages */
    {0, 2048, 2048, 2048, 2048, 0, 0, 0, 0, 0},
    /* ds_p=5: five stages */
    {0, 1638, 1638, 1638, 1638, 1638, 0, 0, 0, 0},
    /* ds_p=6: six stages */
    {0, 1365, 1365, 1365, 1365, 1365, 1365, 0, 0, 0},
    /* ds_p=7: seven stages */
    {0, 1170, 1170, 1170, 1170, 1170, 1170, 1170, 0, 0},
    /* ds_p=8: eight stages */
    {0, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 0},
    /* ds_p=9: nine stages */
    {0, 910, 910, 910, 910, 910, 910, 910, 910, 910},
    /* ds_p=10: ten stages */
    {0, 819, 819, 819, 819, 819, 819, 819, 819, 819}
};

/* Global flag for half-band decimator (should be configurable) */
static int use_halfband_decimator = 1;

/**
 * Half-band decimator for complex interleaved I/Q data.
 * Decimates by 2:1 using symmetric FIR filter.
 *
 * @param in      Input complex samples (interleaved I/Q).
 * @param in_len  Number of complex samples (total elements = 2 * in_len).
 * @param out     Output buffer for decimated complex samples.
 * @param hist_i  Persistent I-channel history of length HB_TAPS-1.
 * @param hist_q  Persistent Q-channel history of length HB_TAPS-1.
 * @return Number of output complex samples.
 */
static int
hb_decim2_complex_interleaved(const int16_t* DSD_FME_RESTRICT in, int in_len, int16_t* DSD_FME_RESTRICT out,
                              int16_t* DSD_FME_RESTRICT hist_i, int16_t* DSD_FME_RESTRICT hist_q) {
    const int hist_len = HB_TAPS - 1;
    int ch_len = in_len >> 1;     /* per-channel samples */
    int out_ch_len = ch_len >> 1; /* decimated per-channel */
    if (out_ch_len <= 0) {
        return 0;
    }
    const int16_t* DSD_FME_RESTRICT in_al = assume_aligned_ptr(in, DSD_FME_ALIGN);
    int16_t* DSD_FME_RESTRICT out_al = assume_aligned_ptr(out, DSD_FME_ALIGN);
    int16_t* DSD_FME_RESTRICT hi = assume_aligned_ptr(hist_i, DSD_FME_ALIGN);
    int16_t* DSD_FME_RESTRICT hq = assume_aligned_ptr(hist_q, DSD_FME_ALIGN);
    int16_t lastI = (ch_len > 0) ? in_al[in_len - 2] : 0;
    int16_t lastQ = (ch_len > 0) ? in_al[in_len - 1] : 0;
    /* Hoist half-band coefficients out of the loop */
    const int16_t c0 = hb_q15_taps[0];
    const int16_t c2 = hb_q15_taps[2];
    const int16_t c4 = hb_q15_taps[4];
    const int16_t c6 = hb_q15_taps[6];
    const int16_t c7 = hb_q15_taps[7];
    for (int n = 0; n < out_ch_len; n++) {
        int center_idx = hist_len + (n << 1); /* per-channel index */
        /* Half-band optimization: only even taps and the center tap contribute (symmetric). */
        auto get_iq = [&](int src_idx, int16_t& xi, int16_t& xq) {
            if (src_idx < hist_len) {
                xi = hi[src_idx];
                xq = hq[src_idx];
            } else {
                int rel = src_idx - hist_len;
                if (rel < ch_len) {
                    xi = in_al[(size_t)(rel << 1)];
                    xq = in_al[(size_t)(rel << 1) + 1];
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
        out_al[(size_t)(n << 1)] = sat16(yI);
        out_al[(size_t)(n << 1) + 1] = sat16(yQ);
    }
    /* Update histories with last HB_TAPS-1 per-channel input samples */
    if (ch_len >= hist_len) {
        int start = ch_len - hist_len;
        for (int k = 0; k < hist_len; k++) {
            int rel = start + k;
            hi[k] = in_al[(size_t)(rel << 1)];
            hq[k] = in_al[(size_t)(rel << 1) + 1];
        }
    } else {
        /* Not enough input samples; pad with zeros */
        int existing = ch_len;
        for (int k = 0; k < hist_len; k++) {
            if (k < existing) {
                int rel = k;
                hi[k] = in_al[(size_t)(rel << 1)];
                hq[k] = in_al[(size_t)(rel << 1) + 1];
            } else {
                hi[k] = 0;
                hq[k] = 0;
            }
        }
    }
    return out_ch_len << 1; /* Return total elements (2 * complex samples) */
}

/* Include the function implementations from rtl_sdr_fm.cpp */
int
low_pass_simple(int16_t* signal2, int len, int step) {
    int i, i2, sum;
    if (step <= 0) {
        return len;
    }
    for (i = 0; i + (step - 1) < len; i += step) {
        sum = 0;
        for (i2 = 0; i2 < step; i2++) {
            sum += (int)signal2[i + i2];
        }
        // Normalize by step with rounding. Writes output at i/step index.
        int val = (sum >= 0) ? (sum + step / 2) / step : -(((-sum) + step / 2) / step);
        signal2[i / step] = (int16_t)val;
    }
    /* Duplicate the final sample to provide one-sample lookahead for callers
	   that expect at least one extra element. Only do this when there is
	   capacity (i.e., out_len < len) to avoid writing past the end. */
    int out_len = len / step;
    if (out_len > 0 && out_len < len) {
        signal2[out_len] = signal2[out_len - 1];
    }
    return out_len;
}

void
low_pass_real(struct demod_state* s) {
    int i = 0, i2 = 0;
    int16_t* r = assume_aligned_ptr(s->result, DSD_FME_ALIGN);
    int fast = (int)s->rate_in;
    int slow = s->rate_out2;
    /* Precompute fixed-point reciprocal of decimation factor to avoid per-sample division */
    int decim = (slow != 0) ? (fast / slow) : 1;
    if (decim < 1) {
        decim = 1;
    }
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

void
low_pass(struct demod_state* d) {
    int i = 0, i2 = 0;
    int16_t* DSD_FME_RESTRICT lp = assume_aligned_ptr(d->lowpassed, DSD_FME_ALIGN);
    while (i < d->lp_len) {
        d->now_r += lp[i];
        d->now_j += lp[i + 1];
        i += 2;
        d->prev_index++;
        if (d->prev_index < d->downsample) {
            continue;
        }
        /* Saturate accumulated sums when writing back to int16 */
        lp[i2] = sat16(d->now_r);
        lp[i2 + 1] = sat16(d->now_j);
        d->prev_index = 0;
        d->now_r = 0;
        d->now_j = 0;
        i2 += 2;
    }
    d->lp_len = i2;
}

void
fifth_order(int16_t* data, int length, int16_t* hist) {
    int i;
    int16_t a, b, c, d, e, f;
    a = hist[1];
    b = hist[2];
    c = hist[3];
    d = hist[4];
    e = hist[5];
    f = data[0];
    /* a downsample should improve resolution, so don't fully shift */
    data[0] = (a + (b + e) * 5 + (c + d) * 10 + f) >> 4;
    for (i = 4; i < length; i += 4) {
        a = c;
        b = d;
        c = e;
        d = f;
        e = data[i - 2];
        f = data[i];
        data[i / 2] = (a + (b + e) * 5 + (c + d) * 10 + f) >> 4;
    }
    /* archive */
    hist[0] = a;
    hist[1] = b;
    hist[2] = c;
    hist[3] = d;
    hist[4] = e;
    hist[5] = f;
}

void
generic_fir(int16_t* data, int length, int* fir, int16_t* hist) {
    int d, temp, sum;
    for (d = 0; d < length; d += 2) {
        temp = data[d];
        sum = 0;
        sum += (hist[0] + hist[8]) * fir[1];
        sum += (hist[1] + hist[7]) * fir[2];
        sum += (hist[2] + hist[6]) * fir[3];
        sum += (hist[3] + hist[5]) * fir[4];
        sum += hist[4] * fir[5];
        sum += (1 << 14); /* Round */
        data[d] = sum >> 15;
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

void
fm_demod(struct demod_state* fm) {
    int i, pcm;
    int16_t* lp = assume_aligned_ptr(fm->lowpassed, DSD_FME_ALIGN);
    int16_t* res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
    /* Use selected discriminator from the very first sample */
    pcm = fm->discriminator(lp[0], lp[1], fm->pre_r, fm->pre_j);
    /* Remove known NCO injection from FLL rotation (demod sees -dphi per sample).
	   Scale Q15 (2*pi==1<<15) to Q14 (pi==1<<14) by >>1. */
    if (fm->fll_enabled) {
        pcm += (fm->fll_freq_q15 >> 1);
    }
    res[0] = (int16_t)pcm;
    DSD_FME_IVDEP
    for (i = 2; i < (fm->lp_len - 1); i += 2) {
        pcm = fm->discriminator(lp[i], lp[i + 1], lp[i - 2], lp[i - 1]);
        if (fm->fll_enabled) {
            pcm += (fm->fll_freq_q15 >> 1);
        }
        res[i / 2] = (int16_t)pcm;
    }
    fm->pre_r = lp[fm->lp_len - 2];
    fm->pre_j = lp[fm->lp_len - 1];
    fm->result_len = fm->lp_len / 2;
}

void
raw_demod(struct demod_state* fm) {
    int i;
    for (i = 0; i < fm->lp_len; i++) {
        fm->result[i] = (int16_t)fm->lowpassed[i];
    }
    fm->result_len = fm->lp_len;
}

void
deemph_filter(struct demod_state* fm) {
    int avg = fm->deemph_avg; /* per-instance state */
    int i, d;
    int16_t* res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
    /* Q15 alpha = (1 - a), where a = exp(-1/(Fs*tau)) */
    const int kShiftDeemph = 15; /* Q15 */
    int alpha_q15 = fm->deemph_a;
    if (alpha_q15 < 0) {
        alpha_q15 = 0;
    }
    if (alpha_q15 > (1 << kShiftDeemph)) {
        alpha_q15 = (1 << kShiftDeemph);
    }
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

void
dc_block_filter(struct demod_state* fm) {
    int i;
    /* Leaky integrator high-pass: dc += (x - dc) >> k; y = x - dc */
    int dc = fm->dc_avg;
    const int k = 11; /* cutoff ~ Fs / 2^k (k in 10..12) */
    int16_t* res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
    DSD_FME_IVDEP
    for (i = 0; i < fm->result_len; i++) {
        int x = (int)res[i];
        dc += (x - dc) >> k;
        int y = x - dc;
        res[i] = sat16(y);
    }
    fm->dc_avg = dc;
}

void
audio_lpf_filter(struct demod_state* fm) {
    if (!fm->audio_lpf_enable) {
        return;
    }
    int i;
    int16_t* res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
    int y = fm->audio_lpf_state;               /* Q0 */
    const int alpha_q15 = fm->audio_lpf_alpha; /* Q15 */
    const int kShift = 15;
    DSD_FME_IVDEP
    for (i = 0; i < fm->result_len; i++) {
        int x = (int)res[i];
        int d = x - y;
        int64_t delta = (int64_t)d * (int64_t)alpha_q15;
        /* symmetric rounding */
        if (d >= 0) {
            delta += (1LL << (kShift - 1));
        } else {
            delta -= (1LL << (kShift - 1));
        }
        y += (int)(delta >> kShift);
        res[i] = (int16_t)y;
    }
    fm->audio_lpf_state = y;
}

long int
mean_power(int16_t* samples, int len, int step) {
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
    if (energy < 0) {
        energy = 0;
    }
    return (long int)(energy / (len > 0 ? len : 1));
}

/* Stub implementations for functions that will be moved in Phase 5 */
/* These will be replaced with actual implementations when Phase 5 is completed */
static void fll_update_error(struct demod_state* d) {
    /* TODO: Implement FLL error estimation */
    (void)d; /* Suppress unused parameter warning */
}

static void fll_mix_and_update(struct demod_state* d) {
    /* TODO: Implement FLL mixing and updating */
    (void)d; /* Suppress unused parameter warning */
}

static void gardner_timing_adjust(struct demod_state* d) {
    /* TODO: Implement Gardner timing adjustment */
    (void)d; /* Suppress unused parameter warning */
}

void
full_demod(struct demod_state* d) {
    int i, ds_p;
    ds_p = d->downsample_passes;
    if (ds_p) {
        /* Choose decimator: half-band cascade (default) or legacy path */
        if (use_halfband_decimator) {
            /* Apply ds_p stages of 2:1 half-band decimation on interleaved lowpassed */
            int in_len = d->lp_len;
            int16_t* src = d->lowpassed;
            int16_t* dst = d->hb_workbuf;
            for (i = 0; i < ds_p; i++) {
                /* Fused complex HB decimation on interleaved I/Q */
                int out_len_interleaved =
                    hb_decim2_complex_interleaved(src, in_len, dst, d->hb_hist_i[i], d->hb_hist_q[i]);
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
            for (i = 0; i < ds_p; i++) {
                fifth_order(d->lowpassed, (d->lp_len >> i), d->lp_i_hist[i]);
                fifth_order(d->lowpassed + 1, (d->lp_len >> i) - 1, d->lp_q_hist[i]);
            }
            d->lp_len = d->lp_len >> ds_p;
            /* droop compensation */
            if (d->comp_fir_size == 9 && ds_p <= CIC_TABLE_MAX) {
                generic_fir(d->lowpassed, d->lp_len, (int*)cic_9_tables[ds_p], d->droop_i_hist);
                generic_fir(d->lowpassed + 1, d->lp_len - 1, (int*)cic_9_tables[ds_p], d->droop_q_hist);
            }
        }
    } else {
        low_pass(d);
    }
    /* Residual CFO loop: estimate error then rotate */
    fll_update_error(d);
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
        /* Ensure even I/Q alignment and accumulate pair power I^2+Q^2 */
        int start = phase & ~1;
        for (int j = start; j + 1 < d->lp_len; j += stride) {
            int64_t ir = (int64_t)d->lowpassed[j];
            int64_t jq = (int64_t)d->lowpassed[j + 1];
            p += ir * ir + jq * jq;
            count++;
        }
        /* Advance phase to sample different positions next block */
        if (stride > 0) {
            int adv = d->lp_len % stride;
            /* keep even alignment for I/Q pairing */
            if (adv & 1) {
                adv++;
            }
            if (adv >= stride) {
                adv %= stride;
            }
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
                while ((1 << shift) < w && shift < 30) {
                    shift++;
                }
                int64_t delta = (block_mean - d->squelch_running_power);
                d->squelch_running_power += (delta >> shift);
            }
        }
        int64_t thr2 = (int64_t)d->squelch_level * (int64_t)d->squelch_level;
        if (d->squelch_running_power < thr2) {
            d->squelch_hits++;
            for (i = 0; i < d->lp_len; i++) {
                d->lowpassed[i] = 0;
            }
        } else {
            d->squelch_hits = 0;
        }
    }
    d->mode_demod(d); /* lowpassed -> result */
    if (d->mode_demod == &raw_demod) {
        return;
    }
    /* todo, fm noise squelch */
    // use nicer filter here too?
    if (d->post_downsample > 1) {
        d->result_len = low_pass_simple(d->result, d->result_len, d->post_downsample);
    }
    if (d->deemph) {
        deemph_filter(d);
    }
    /* Optional post-demod audio LPF */
    audio_lpf_filter(d);
    if (d->dc_block) {
        dc_block_filter(d);
    }
    if (d->rate_out2 > 0) {
        low_pass_real(d);
        //arbitrary_resample(d->result, d->result, d->result_len, d->result_len * d->rate_out2 / d->rate_out);
    }
}
