/*
 * Frequency-Locked Loop Implementation
 *
 * This file implements the residual carrier frequency correction system
 * using a digital frequency-locked loop. It provides automatic frequency
 * offset compensation with configurable loop parameters and sine LUT
 * optimization for improved performance.
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

#include "dsp/fll.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

/* Quarter-wave sine LUT for FLL rotator (Q15). */
static int16_t fll_qsine_q15_lut[1025]; /* 0..pi/2 in 1024 steps, +1 guard for exact pi/2 */
static pthread_once_t fll_lut_once = PTHREAD_ONCE_INIT;

/* Build quarter-wave sine LUT in Q15: sin(theta) where theta in [0, pi/2] */
static void
fll_lut_once_init(void) {
    for (int i = 0; i <= 1024; i++) {
        double theta = (double)i * M_PI / 2.0 / 1024.0;
        double v = sin(theta) * 32767.0;
        if (v > 32767.0) {
            v = 32767.0;
        }
        if (v < -32767.0) {
            v = -32767.0;
        }
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
static void
fll_sin_cos_q15_from_phase_lut(int phase_q15, int16_t* c_out, int16_t* s_out) {
    /* phase_q15 wraps at 1<<15 mapping to 2*pi */
    int p = phase_q15 & 0x7FFF; /* 0..32767 */
    int quad = p >> 13;         /* 0..3 */
    int r = p & 0x1FFF;         /* position within quadrant: 0..8191 */

    /* Helper to sample quarter-wave S(r) with r in [0..8192] using 1024-segment linear interp */
    auto sample_quarter = [](int r8192) -> int16_t {
        if (r8192 < 0) {
            r8192 = 0;
        }
        if (r8192 > 8192) {
            r8192 = 8192;
        }
        int idx = r8192 >> 3; /* 0..1024 */
        int frac = r8192 & 7; /* 0..7 */
        int16_t s0 = fll_qsine_q15_lut[idx];
        int16_t s1 = fll_qsine_q15_lut[(idx < 1024) ? (idx + 1) : 1024];
        int diff = (int)s1 - (int)s0;
        int interp = (int)s0 + ((diff * frac + 4) >> 3); /* rounded */
        if (interp > 32767) {
            interp = 32767;
        }
        if (interp < -32767) {
            interp = -32767;
        }
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

/* Fast complex multiply for polar discriminator */
static inline void
multiply64(int ar, int aj, int br, int bj, int64_t* cr, int64_t* cj) {
    *cr = (int64_t)ar * (int64_t)br - (int64_t)aj * (int64_t)bj;
    *cj = (int64_t)aj * (int64_t)br + (int64_t)ar * (int64_t)bj;
}

/* Fast atan2 approximation for 64-bit inputs */
static int
fast_atan2_64(int64_t y, int64_t x) {
    int angle;
    int pi4 = (1 << 12), pi34 = 3 * (1 << 12); /* note: pi = 1<<14 */
    int64_t yabs;
    if (x == 0 && y == 0) {
        return 0;
    }
    yabs = (y < 0) ? -y : y;

    if (x >= 0) {
        /* Use stable form: pi/4 - pi/4 * (x - |y|) / (x + |y|) */
        int64_t denom = x + yabs; /* only zero when x==0 && y==0 handled above */
        if (denom == 0) {
            angle = 0;
        } else {
            angle = (int)(pi4 - ((int64_t)pi4 * (x - yabs)) / denom);
        }
    } else {
        /* Use stable form: 3pi/4 - pi/4 * (x + |y|) / (|y| - x) */
        int64_t denom = yabs - x; /* strictly > 0 for x < 0 */
        if (denom == 0) {
            angle = pi34; /* rare tie case; pick quadrant boundary */
        } else {
            angle = (int)(pi34 - ((int64_t)pi4 * (x + yabs)) / denom);
        }
    }
    if (y < 0) {
        return -angle;
    }
    return angle;
}

/* Polar discriminator using fast atan2 approximation */
static int
polar_disc_fast(int ar, int aj, int br, int bj) {
    int64_t cr, cj;
    multiply64(ar, aj, br, -bj, &cr, &cj);
    return fast_atan2_64(cj, cr);
}

/**
 * Initialize FLL state with default values
 */
void
fll_init_state(fll_state_t* state) {
    state->freq_q15 = 0;
    state->phase_q15 = 0;
    state->prev_r = 0;
    state->prev_j = 0;
}

/**
 * Mix lowpassed I/Q by NCO e^{j*phi}, update phase by freq_q15 per sample.
 * Phase and frequency are Q15 where a full turn (2*pi) maps to 1<<15.
 *
 * @param config FLL configuration
 * @param state  FLL state (updates phase_q15)
 * @param x      Input/output I/Q buffer (modified in-place)
 * @param N      Length of buffer (must be even)
 */
void
fll_mix_and_update(const fll_config_t* config, fll_state_t* state, int16_t* x, int N) {
    if (!config->enabled) {
        return;
    }

    int phase = state->phase_q15;     /* Q15 wraps at 1<<15 ~ 2*pi */
    const int freq = state->freq_q15; /* Q15 increment per sample */

    /* Optional: higher-quality quarter-wave LUT rotator (linear interp) */
    if (config->use_lut) {
        /* Ensure LUT is initialized once before use */
        pthread_once(&fll_lut_once, fll_lut_once_init);
        for (int i = 0; i + 1 < N; i += 2) {
            int16_t c, s;
            fll_sin_cos_q15_from_phase_lut(phase, &c, &s);
            int xr = x[i];
            int xj = x[i + 1];
            int32_t yr = ((int32_t)xr * c + (int32_t)xj * s) >> 15;
            int32_t yj = ((int32_t)xj * c - (int32_t)xr * s) >> 15;
            x[i] = (int16_t)yr;
            x[i + 1] = (int16_t)yj;
            phase += freq;
        }
    } else {
        /* Fast LUT-free rotator: piecewise-linear sin/cos within quadrants.
         * Fix amplitude symmetry to better match Q15 sin/cos.
         */
        for (int i = 0; i + 1 < N; i += 2) {
            int p = phase & 0x7FFF; /* 0..32767 */
            int q = p >> 13;        /* quadrant 0..3 */
            int r = p & 0x1FFF;     /* 0..8191 */
            /* Linearized quarter-wave; ensure consistent endpoints */
            int16_t s_pos = (int16_t)(r << 2);        /* 0..32764 */
            int16_t c_pos = (int16_t)(32767 - s_pos); /* 32767..3 */
            int16_t s, c;
            switch (q) {
                case 0:
                    s = s_pos;
                    c = c_pos;
                    break;
                case 1:
                    s = c_pos;
                    c = (int16_t)(-s_pos);
                    break;
                case 2:
                    s = (int16_t)(-s_pos);
                    c = (int16_t)(-c_pos);
                    break;
                default:
                    s = (int16_t)(-c_pos);
                    c = s_pos;
                    break;
            }
            int xr = x[i];
            int xj = x[i + 1];
            int32_t yr = ((int32_t)xr * c + (int32_t)xj * s) >> 15;
            int32_t yj = ((int32_t)xj * c - (int32_t)xr * s) >> 15;
            x[i] = (int16_t)yr;
            x[i + 1] = (int16_t)yj;
            phase += freq;
        }
    }
    state->phase_q15 = phase & 0x7FFF;
}

/**
 * Estimate frequency error using a simple phase-difference discriminator and
 * update the FLL control in Q15. The proportional term is applied directly
 * and the integral action is realized by accumulating into freq_q15.
 *
 * @param config FLL configuration
 * @param state  FLL state (updates freq_q15 and phase_q15)
 * @param x      Input I/Q buffer
 * @param N      Length of buffer (must be even)
 */
void
fll_update_error(const fll_config_t* config, fll_state_t* state, const int16_t* x, int N) {
    if (!config->enabled) {
        return;
    }

    int alpha = config->alpha_q15; /* Q15 */
    int beta = config->beta_q15;   /* Q15 */
    int prev_r = state->prev_r;
    int prev_j = state->prev_j;
    int32_t err_acc = 0;
    int count = 0;

    for (int i = 0; i + 1 < N; i += 2) {
        int r = x[i];
        int j = x[i + 1];
        if (i > 0 || (prev_r != 0 || prev_j != 0)) {
            int e = polar_disc_fast(r, j, prev_r, prev_j); /* Q14 */
            err_acc += e;
            count++;
        }
        prev_r = r;
        prev_j = j;
    }

    state->prev_r = prev_r;
    state->prev_j = prev_j;

    if (count == 0) {
        return;
    }

    int32_t err = err_acc / count; /* Q14 */

    /* Deadband: ignore tiny phase errors to avoid audible low-frequency ramps */
    if (err < config->deadband_q14 && err > -config->deadband_q14) {
        return;
    }

    /* Standard PI loop: adjust frequency only (no direct phase steps). */
    int32_t p = ((int64_t)alpha * err) >> 14;   /* -> Q15 */
    int32_t iacc = ((int64_t)beta * err) >> 14; /* -> Q15 */
    int32_t df = p + iacc;                      /* Q15 */

    /* Negative feedback */
    /* Slew-rate limit */
    if (df > config->slew_max_q15) {
        df = config->slew_max_q15;
    }
    if (df < -config->slew_max_q15) {
        df = -config->slew_max_q15;
    }

    state->freq_q15 += (int)df;

    /* Clamp NCO frequency to safe range */
    {
        const int32_t F_CLAMP = 2048; /* allow up to ~±3 kHz @48k */
        if (state->freq_q15 > F_CLAMP) {
            state->freq_q15 = F_CLAMP;
        }
        if (state->freq_q15 < -F_CLAMP) {
            state->freq_q15 = -F_CLAMP;
        }
    }
}
