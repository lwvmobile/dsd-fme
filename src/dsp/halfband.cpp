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
 * @brief Half-band FIR decimator implementation for real sequences.
 *
 * Implements Q15 half-band decimation by 2 with persistent history.
 */

#include <stdint.h>
#include <string.h>

#include "dsp/halfband.h"
#include "dsp/math_utils.h"

/* Q15-scaled half-band coefficients (see declaration in header) */
const int16_t hb_q15_taps[HB_TAPS] = {-108, 0, 1800, 0, -500, 0, 7000, 16384, 7000, 0, -500, 0, 1800, 0, -108};

/**
 * @brief Decimate one real-valued channel by 2 using a half-band FIR.
 *
 * Applies a 15-tap half-band low-pass to input samples and writes every second
 * filtered sample to the output. Maintains a left-wing history across calls to
 * preserve continuity at block boundaries.
 *
 * @param in Pointer to real input samples (length in_len).
 * @param in_len Number of input samples.
 * @param out Output buffer, size must be at least in_len/2.
 * @param hist Persistent history of length HB_TAPS-1 (left wing).
 * @return Number of output samples written (in_len/2).
 */
int
hb_decim2_real(const int16_t* in, int in_len, int16_t* out, int16_t* hist) {
    const int hist_len = HB_TAPS - 1;
    int16_t last = (in_len > 0) ? in[in_len - 1] : 0;
    int out_len = in_len >> 1; /* floor */
    const int16_t c0 = hb_q15_taps[0];
    const int16_t c2 = hb_q15_taps[2];
    const int16_t c4 = hb_q15_taps[4];
    const int16_t c6 = hb_q15_taps[6];
    const int16_t c7 = hb_q15_taps[7];
    for (int n = 0; n < out_len; n++) {
        int center_idx = hist_len + (n << 1);
        auto get_sample = [&](int src_idx) -> int16_t {
            if (src_idx < hist_len) {
                return hist[src_idx];
            } else {
                int rel = src_idx - hist_len;
                return (rel < in_len) ? in[rel] : last;
            }
        };
        int16_t xc = get_sample(center_idx);
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
        acc += (1 << 14);
        int32_t y = (int32_t)(acc >> 15);
        out[n] = sat16(y);
    }
    if (in_len >= hist_len) {
        memcpy(hist, in + (in_len - hist_len), (size_t)hist_len * sizeof(int16_t));
    } else {
        int need = hist_len - in_len;
        if (need > 0) {
            memmove(hist, hist + in_len, (size_t)need * sizeof(int16_t));
        }
        memcpy(hist + need, in, (size_t)in_len * sizeof(int16_t));
    }
    return out_len;
}
