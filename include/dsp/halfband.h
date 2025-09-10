/*
 * Half-band decimation filters (Q15 coefficients)
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
#pragma once

#include <stdint.h>

/**
 * Number of taps for the half-band FIR low-pass filter.
 */
#define HB_TAPS 15

/**
 * Q15-scaled symmetric half-band coefficients.
 *
 * Odd-indexed taps are zero; the center tap is 0.5 (16384). The remaining even
 * taps sum to 0.5 to yield unity DC gain.
 */
extern const int16_t hb_q15_taps[HB_TAPS];

/**
 * Decimate a real-valued sequence by 2 using a half-band FIR.
 *
 * Applies a 15-tap half-band low-pass to input samples and writes every second
 * filtered sample to the output. Maintains a left-wing history across calls to
 * preserve continuity at block boundaries.
 *
 * @param in     Pointer to real input samples (length in_len).
 * @param in_len Number of input samples.
 * @param out    Output buffer, size must be at least in_len/2.
 * @param hist   Persistent history of length HB_TAPS-1 (left wing).
 * @return Number of output samples written (in_len/2).
 */
int hb_decim2_real(const int16_t* in, int in_len, int16_t* out, int16_t* hist);
