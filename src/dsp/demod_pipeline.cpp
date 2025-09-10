/*
 * DSP Demodulation Pipeline Implementation
 *
 * This file implements the complete FM demodulation pipeline, including
 * low-pass filtering, FM discrimination, deemphasis, DC blocking, and
 * audio filtering. It orchestrates the signal processing chain from
 * baseband IQ samples to final audio output.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp/demod_pipeline.h"
#include "dsp/demod_state.h"
#include "dsp/fll.h"
#include "dsp/halfband.h"
#include "dsp/math_utils.h"
#include "dsp/ted.h"

/* demod_state now provided by include/dsp/demod_state.h */

/* Macros and constants from the original file */
#ifndef MAXIMUM_OVERSAMPLE
#define MAXIMUM_OVERSAMPLE 16
#endif
#ifndef DEFAULT_BUF_LENGTH
#define DEFAULT_BUF_LENGTH 16384
#endif
#ifndef MAXIMUM_BUF_LENGTH
#define MAXIMUM_BUF_LENGTH (MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#endif
#ifndef MAX_BANDWIDTH_MULTIPLIER
#define MAX_BANDWIDTH_MULTIPLIER 8
#endif

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
template <typename T>
static inline T*
assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return p;
}

template <typename T>
static inline const T*
assume_aligned_ptr(const T* p, size_t /*align_unused*/) {
    return p;
}

/* HB_TAPS and hb_q15_taps provided by dsp/halfband.h */

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
    {0, 819, 819, 819, 819, 819, 819, 819, 819, 819}};

/* Global flags provided by rtl front-end */
extern int use_halfband_decimator;
extern int fll_lut_enabled;

/**
 * Decimate one real channel by 2 using a half-band FIR with persistent left history.
 *
 * @param in   Pointer to real input samples.
 * @param in_len Number of real input samples.
 * @param out  Pointer to output buffer (size >= in_len/2).
 * @param hist Persistent history of length HB_TAPS-1 (left wing).
 * @return Number of output samples written (in_len/2).
 */
/* hb_decim2_real provided by dsp/halfband.h */

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

/**
 * Boxcar low-pass and decimate by step (no wraparound).
 * Length must be a multiple of step.
 *
 * @param signal2 In/out buffer of samples.
 * @param len     Length of input buffer.
 * @param step    Decimation factor.
 * @return New length after decimation.
 */
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

/**
 * Simple square window FIR on real samples with decimation to rate_out2.
 *
 * @param s Demodulator state (uses result buffer and decimation state).
 */
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

/**
 * Deferred low-pass: sums and decimates with saturation on writeback.
 *
 * @param d Demodulator state (uses lowpassed buffer and decimation state).
 */
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

/**
 * Fifth-order half-band-like decimator operating on a single real sequence.
 * Caller applies this separately to I and Q streams. Uses 6-tap state in
 * `hist` and writes decimated output in-place.
 *
 * @param data   In/out real data buffer (single channel).
 * @param length Input length (elements), processed in-place.
 * @param hist   Persistent history buffer of length >= 6.
 */
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

/**
 * FIR filter with symmetric 9-tap coefficients (phase-saving implementation).
 *
 * @param data   In/out data buffer (interleaved step of 2 assumed).
 * @param length Number of input samples.
 * @param fir    Coefficient array (expects layout for length 9).
 * @param hist   History buffer used across calls.
 */
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

/**
 * Perform FM discriminator on interleaved low-passed I/Q to produce audio PCM.
 * Uses the active discriminator configured in fm->discriminator.
 *
 * @param fm Demodulator state (uses lowpassed as input, writes to result).
 */
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

/**
 * Pass-through demodulator: copies low-passed samples to output unchanged.
 *
 * @param fm Demodulator state (copies lowpassed to result).
 */
void
raw_demod(struct demod_state* fm) {
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

/**
 * Apply a simple DC blocking (leaky integrator high-pass) filter to audio.
 *
 * @param fm Demodulator state (reads/writes result, updates dc_avg).
 */
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

/**
 * Apply a simple one-pole low-pass filter to audio.
 *
 * @param fm Demodulator state (reads/writes result, updates audio_lpf_state).
 */
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

/**
 * Calculate mean power (squared RMS) with DC bias removed.
 *
 * @param samples Input samples buffer.
 * @param len     Number of samples.
 * @param step    Step size for sampling.
 * @return Mean power (squared RMS) with DC bias removed.
 */
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

/**
 * Estimate frequency error using the configured discriminator and update the
 * FLL loop control variables in Q15. Mirrors the modular FLL path used by the
 * RTL front-end.
 *
 * @param d Demodulator state (syncs to/from `fll_state`, updates loop vars).
 */
static void
fll_update_error(struct demod_state* d) {
    if (!d->fll_enabled) {
        return;
    }
    /* Sync from demod_state to module state */
    d->fll_state.freq_q15 = d->fll_freq_q15;
    d->fll_state.phase_q15 = d->fll_phase_q15;
    d->fll_state.prev_r = d->fll_prev_r;
    d->fll_state.prev_j = d->fll_prev_j;

    fll_config_t cfg = {.enabled = d->fll_enabled,
                        .alpha_q15 = d->fll_alpha_q15,
                        .beta_q15 = d->fll_beta_q15,
                        .deadband_q14 = d->fll_deadband_q14,
                        .slew_max_q15 = d->fll_slew_max_q15,
                        .use_lut = fll_lut_enabled};

    fll_update_error(&cfg, &d->fll_state, d->lowpassed, d->lp_len);

    /* Sync back to demod_state */
    d->fll_freq_q15 = d->fll_state.freq_q15;
    d->fll_phase_q15 = d->fll_state.phase_q15;
    d->fll_prev_r = d->fll_state.prev_r;
    d->fll_prev_j = d->fll_state.prev_j;
}

/**
 * Mix low-passed I/Q by the FLL NCO and advance the loop accumulators. This
 * rotates the complex baseband to reduce residual CFO and synchronizes the
 * demod state with the modular FLL implementation.
 *
 * @param d Demodulator state (reads/writes `lowpassed`, updates `fll_state`).
 */
static void
fll_mix_and_update(struct demod_state* d) {
    if (!d->fll_enabled) {
        return;
    }

    /* Sync from demod_state to module state */
    d->fll_state.freq_q15 = d->fll_freq_q15;
    d->fll_state.phase_q15 = d->fll_phase_q15;
    d->fll_state.prev_r = d->fll_prev_r;
    d->fll_state.prev_j = d->fll_prev_j;

    fll_config_t cfg = {.enabled = d->fll_enabled,
                        .alpha_q15 = d->fll_alpha_q15,
                        .beta_q15 = d->fll_beta_q15,
                        .deadband_q14 = d->fll_deadband_q14,
                        .slew_max_q15 = d->fll_slew_max_q15,
                        .use_lut = fll_lut_enabled};

    fll_mix_and_update(&cfg, &d->fll_state, d->lowpassed, d->lp_len);

    /* Sync back to demod_state */
    d->fll_freq_q15 = d->fll_state.freq_q15;
    d->fll_phase_q15 = d->fll_state.phase_q15;
    d->fll_prev_r = d->fll_state.prev_r;
    d->fll_prev_j = d->fll_state.prev_j;
}

/**
 * Apply a lightweight Gardner timing correction to complex baseband. When
 * enabled, this may adjust `lowpassed` and `lp_len` via the modular TED API.
 * Intended primarily for digital modes; typically disabled for analog FM.
 *
 * @param d Demodulator state (syncs `ted_state`, may modify samples/length).
 */
static void
gardner_timing_adjust(struct demod_state* d) {
    if (!d->ted_enabled || d->ted_sps <= 1) {
        return;
    }

    /* Sync from demod_state to module state */
    d->ted_state.mu_q20 = d->ted_mu_q20;

    ted_config_t cfg = {
        .enabled = d->ted_enabled, .force = d->ted_force, .gain_q20 = d->ted_gain_q20, .sps = d->ted_sps};

    gardner_timing_adjust(&cfg, &d->ted_state, d->lowpassed, &d->lp_len, d->timing_buf);

    /* Sync back to demod_state */
    d->ted_mu_q20 = d->ted_state.mu_q20;
}

/**
 * Full demodulation pipeline for one block.
 * Applies decimation (HB cascade or legacy), optional FLL and timing
 * correction, followed by the configured discriminator and post-processing.
 *
 * @param d Demodulator state (consumes lowpassed, produces result).
 */
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
