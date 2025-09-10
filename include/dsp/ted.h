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
 * @brief Timing Error Detector (TED) interface: Gardner TED and fractional
 * delay timing correction for symbol synchronization in digital demodulation modes.
 */

#ifndef DSP_TED_H
#define DSP_TED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TED Configuration structure */
typedef struct {
    int enabled;
    int force;    /* allow forcing TED even for FM/C4FM paths */
    int gain_q20; /* small gain (Q20) for stability */
    int sps;      /* nominal samples per symbol (e.g., 10 for 4800 sym/s at 48k) */
} ted_config_t;

/* TED State structure - minimal fields needed for TED operations */
typedef struct {
    int mu_q20; /* fractional phase [0,1) in Q20 */
} ted_state_t;

/**
 * @brief Initialize TED state with default values.
 *
 * @param state TED state to initialize.
 */
void ted_init_state(ted_state_t* state);

/**
 * @brief Lightweight Gardner timing correction.
 *
 * Uses linear interpolation between adjacent complex samples around the
 * nominal samples-per-symbol to reduce timing error; intended for digital
 * modes when enabled.
 *
 * @param config TED configuration.
 * @param state  TED state (updates mu_q20).
 * @param x      Input/output I/Q buffer (modified in-place if timing adjustment applied).
 * @param N      Length of buffer (must be even; updated if timing adjustment applied).
 * @param y      Work buffer for timing-adjusted I/Q (must be at least size N).
 * @note Skips processing when samples-per-symbol is large unless forced.
 */
void gardner_timing_adjust(const ted_config_t* config, ted_state_t* state, int16_t* x, int* N, int16_t* y);

#ifdef __cplusplus
}
#endif

#endif /* DSP_TED_H */
