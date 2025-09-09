/*
 * Frequency-Locked Loop (FLL) Header
 *
 * This header defines the interface for the residual carrier frequency
 * correction system using a frequency-locked loop. It provides functions
 * for automatic frequency correction in FM demodulation to compensate for
 * frequency offsets and drifts.
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

#ifndef DSP_FLL_H
#define DSP_FLL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FLL Configuration structure */
typedef struct {
    int enabled;
    int alpha_q15;    /* proportional gain (Q15) */
    int beta_q15;     /* integral gain (Q15) */
    int deadband_q14; /* ignore small phase errors |err| <= deadband (Q14) */
    int slew_max_q15; /* max |delta freq| per update (Q15) */
    int use_lut;      /* use LUT-based rotator instead of fast approximation */
} fll_config_t;

/* FLL State structure - minimal fields needed for FLL operations */
typedef struct {
    int freq_q15;  /* NCO frequency increment (Q15 radians/sample scaled) */
    int phase_q15; /* NCO phase accumulator (wrap at 2*pi -> 1<<15 scale) */
    int prev_r;
    int prev_j;
} fll_state_t;

/**
 * Initialize FLL state with default values
 */
void fll_init_state(fll_state_t* state);

/**
 * Mix lowpassed I/Q by NCO e^{j*phi}, update phase by freq_q15 per sample.
 * Phase and frequency are Q15 where a full turn (2*pi) maps to 1<<15.
 *
 * @param config FLL configuration
 * @param state  FLL state (updates phase_q15)
 * @param x      Input/output I/Q buffer (modified in-place)
 * @param N      Length of buffer (must be even)
 */
void fll_mix_and_update(const fll_config_t* config, fll_state_t* state, int16_t* x, int N);

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
void fll_update_error(const fll_config_t* config, fll_state_t* state, const int16_t* x, int N);

#ifdef __cplusplus
}
#endif

#endif /* DSP_FLL_H */
