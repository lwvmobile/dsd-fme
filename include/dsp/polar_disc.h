/*
 * Polar discriminator helpers (FM phase difference estimators)
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
 * Initialize the atan lookup table used by the LUT-based discriminator.
 *
 * Thread-safe and idempotent. If memory cannot be allocated, the function
 * leaves the LUT disabled and returns -1.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int atan_lut_init(void);
/**
 * Free memory associated with the atan LUT.
 *
 * Safe to call multiple times.
 */
void atan_lut_free(void);

/**
 * Accurate polar discriminator using double-precision atan2.
 *
 * Computes b * conj(a) and returns the phase delta in Q14 where
 * pi == 1<<14.
 *
 * @param ar Real part of previous complex sample a.
 * @param aj Imag part of previous complex sample a.
 * @param br Real part of current complex sample b.
 * @param bj Imag part of current complex sample b.
 * @return Phase difference in Q14 units.
 */
int polar_discriminant(int ar, int aj, int br, int bj);
/**
 * Fast polar discriminator using an integer atan2 approximation.
 *
 * Uses a low-cost integer approximation to atan2 with 64-bit safety.
 * Returns the phase delta in Q14 where pi == 1<<14.
 */
int polar_disc_fast(int ar, int aj, int br, int bj);
/**
 * LUT-based polar discriminator.
 *
 * Uses a precomputed atan LUT to approximate atan2. Falls back to the fast
 * integer approximation if the LUT is not available.
 */
int polar_disc_lut(int ar, int aj, int br, int bj);
