/*
 * Worker Pool Header
 *
 * This header defines the interface for a minimal 2-thread worker pool
 * used for CPU-intensive inner loops in the demodulation pipeline. It
 * provides thread-safe task distribution for parallel processing when
 * enabled via runtime configuration.
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

#ifndef RUNTIME_WORKER_POOL_H
#define RUNTIME_WORKER_POOL_H

#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration to avoid including heavy headers here */
struct demod_state;

/*
 * Minimal 2-thread worker pool API (env-gated by DSD_FME_MT)
 * These calls mirror the inlined versions that previously lived in rtl_sdr_fm.cpp
 */

/* Initialize the minimal worker pool if DSD_FME_MT=1. Safe to call multiple times per instance. */
void demod_mt_init(struct demod_state* s);

/* Tear down the worker threads if they were created by demod_mt_init. */
void demod_mt_destroy(struct demod_state* s);

/* Post up to two tasks and wait for completion. Runs synchronously if pool disabled. */
void demod_mt_run_two(struct demod_state* s, void (*f0)(void*), void* a0, void (*f1)(void*), void* a1);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_WORKER_POOL_H */
