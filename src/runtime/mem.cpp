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
 * @brief Aligned memory allocation utilities for DSP operations.
 *
 * Provides `dsd_fme_aligned_malloc` and `dsd_fme_aligned_free` with a default
 * alignment of `DSD_FME_ALIGN` for SIMD-friendly and cache-efficient buffers.
 */

#include <cstdlib>

#include "runtime/mem.h"

/**
 * @brief Allocate memory aligned to `DSD_FME_ALIGN`.
 *
 * Falls back to `malloc` if an aligned allocation API is unavailable.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure or when `size` is 0.
 */
void*
dsd_fme_aligned_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
    void* mem_ptr = NULL;
    if (posix_memalign(&mem_ptr, DSD_FME_ALIGN, size) != 0) {
        mem_ptr = std::malloc(size);
    }
    return mem_ptr;
#else
    return std::malloc(size);
#endif
}

/**
 * @brief Free memory allocated by `dsd_fme_aligned_malloc`.
 *
 * Also valid for memory allocated by the plain `malloc` fallback.
 *
 * @param ptr Pointer previously returned by `dsd_fme_aligned_malloc`.
 */
void
dsd_fme_aligned_free(void* ptr) {
    std::free(ptr);
}
