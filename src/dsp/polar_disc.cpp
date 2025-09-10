/*
 * Polar discriminator implementations and LUT management.
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

#include "dsp/polar_disc.h"

static const double kPi = 3.14159265358979323846;

static int* atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;
static pthread_once_t atan_lut_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t atan_lut_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Multiply two complex numbers using 64-bit intermediates to prevent overflow.
 *
 * (ar + j*aj) * (br + j*bj) -> (cr + j*cj)
 *
 * @param ar Real part of a.
 * @param aj Imag part of a.
 * @param br Real part of b.
 * @param bj Imag part of b.
 * @param cr [out] Real part of result (int64).
 * @param cj [out] Imag part of result (int64).
 */
static inline void
multiply64(int ar, int aj, int br, int bj, int64_t* cr, int64_t* cj) {
    *cr = (int64_t)ar * (int64_t)br - (int64_t)aj * (int64_t)bj;
    *cj = (int64_t)aj * (int64_t)br + (int64_t)ar * (int64_t)bj;
}

/**
 * Initialize the atan LUT contents (invoked via pthread_once).
 */
static void
atan_lut_once_init(void) {
    int i;
    atan_lut = (int*)malloc((size_t)atan_lut_size * sizeof(int));
    if (atan_lut == NULL) {
        return;
    }
    for (i = 0; i < atan_lut_size; i++) {
        atan_lut[i] = (int)(atan((double)i / (1 << atan_lut_coef)) / kPi * (1 << 14));
    }
}

/**
 * Initialize the atan lookup table used by the LUT-based discriminator.
 * Thread-safe and idempotent; returns -1 if allocation fails.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int
atan_lut_init(void) {
    pthread_once(&atan_lut_once, atan_lut_once_init);
    if (atan_lut != NULL) {
        return 0;
    }
    pthread_mutex_lock(&atan_lut_mutex);
    if (atan_lut == NULL) {
        atan_lut_once_init();
    }
    pthread_mutex_unlock(&atan_lut_mutex);
    return (atan_lut != NULL) ? 0 : -1;
}

/**
 * Free memory associated with the atan LUT. Safe to call multiple times.
 */
void
atan_lut_free(void) {
    pthread_mutex_lock(&atan_lut_mutex);
    if (atan_lut != NULL) {
        free(atan_lut);
        atan_lut = NULL;
    }
    pthread_mutex_unlock(&atan_lut_mutex);
}

/**
 * Fast integer atan2 approximation (Q14) with 64-bit safety.
 *
 * @param y Imaginary component (int64).
 * @param x Real component (int64).
 * @return Angle where pi == 1<<14 (Q14 scaling).
 */
static inline int
fast_atan2_64(int64_t y, int64_t x) {
    int angle;
    int pi4 = (1 << 12), pi34 = 3 * (1 << 12); /* note: pi = 1<<14 */
    int64_t yabs;
    if (x == 0 && y == 0) {
        return 0;
    }
    yabs = y;
    if (yabs < 0) {
        yabs = -yabs;
    }
    if (x >= 0) {
        angle = (int)(pi4 - ((int64_t)pi4 * (x - yabs)) / (x + yabs));
    } else {
        angle = (int)(pi34 - ((int64_t)pi4 * (x + yabs)) / (yabs - x));
    }
    if (y < 0) {
        return -angle;
    }
    return angle;
}

/**
 * Accurate polar discriminator using double-precision atan2.
 *
 * Computes b * conj(a) and returns the phase delta in Q14 where pi == 1<<14.
 *
 * @param ar Real part of previous complex sample a.
 * @param aj Imag part of previous complex sample a.
 * @param br Real part of current complex sample b.
 * @param bj Imag part of current complex sample b.
 * @return Phase difference in Q14 units.
 */
int
polar_discriminant(int ar, int aj, int br, int bj) {
    int64_t cr, cj;
    double angle;
    multiply64(ar, aj, br, -bj, &cr, &cj);
    angle = atan2((double)cj, (double)cr);
    return (int)(angle / kPi * (1 << 14));
}

/**
 * Fast polar discriminator using an integer atan2 approximation.
 *
 * Uses a low-cost integer approximation to atan2 with 64-bit safety.
 * Returns the phase delta in Q14 where pi == 1<<14.
 */
int
polar_disc_fast(int ar, int aj, int br, int bj) {
    int64_t cr, cj;
    multiply64(ar, aj, br, -bj, &cr, &cj);
    return fast_atan2_64(cj, cr);
}

/**
 * LUT-based polar discriminator.
 *
 * Uses a precomputed atan LUT to approximate atan2 for b * conj(a).
 * Falls back to the fast integer approximation if the LUT is not
 * available. Returns the phase delta in Q14 where pi == 1<<14.
 *
 * @return Phase difference in Q14 units.
 */
int
polar_disc_lut(int ar, int aj, int br, int bj) {
    int64_t cr, cj;
    int64_t x, x_abs;

    atan_lut_init();
    if (atan_lut == NULL) {
        multiply64(ar, aj, br, -bj, &cr, &cj);
        return fast_atan2_64(cj, cr);
    }

    multiply64(ar, aj, br, -bj, &cr, &cj);

    if (cr == 0 || cj == 0) {
        if (cr == 0 && cj == 0) {
            return 0;
        }
        if (cr == 0 && cj > 0) {
            return 1 << 13;
        }
        if (cr == 0 && cj < 0) {
            return -(1 << 13);
        }
        if (cj == 0 && cr > 0) {
            return 0;
        }
        if (cj == 0 && cr < 0) {
            return (1 << 14) - 1;
        }
    }

    x = ((int64_t)cj << atan_lut_coef) / cr;
    x_abs = (x < 0) ? -x : x;

    if (x_abs >= (int64_t)atan_lut_size) {
        if (cr < 0) {
            return (cj >= 0) ? ((1 << 14) - 1) : (-(1 << 14) + 1);
        } else {
            return (cj >= 0) ? (1 << 13) : -(1 << 13);
        }
    }

    if (x > 0) {
        int val = (cj > 0) ? atan_lut[(int)x] : (atan_lut[(int)x] - (1 << 14));
        if (val == (1 << 14)) {
            val = (1 << 14) - 1;
        }
        if (val == -(1 << 14)) {
            val = -(1 << 14) + 1;
        }
        return val;
    } else {
        int val = (cj > 0) ? ((1 << 14) - atan_lut[(int)(-x)]) : (-atan_lut[(int)(-x)]);
        if (val == (1 << 14)) {
            val = (1 << 14) - 1;
        }
        if (val == -(1 << 14)) {
            val = -(1 << 14) + 1;
        }
        return val;
    }

    return 0;
}
