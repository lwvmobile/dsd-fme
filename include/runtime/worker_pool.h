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
 * @brief Minimal 2-thread worker pool API for intra-block demodulation tasks.
 *
 * Exposes functions keyed by `demod_state*` to initialize/destroy a tiny
 * env-gated pool and to run up to two tasks in parallel per processing block.
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

/**
 * @brief Initialize the minimal worker pool when `DSD_FME_MT=1`.
 *
 * Safe to call multiple times per demodulator instance.
 * @param s Demodulator state used as a key for the worker context.
 * @note No-op when multithreading is disabled via environment.
 */
void demod_mt_init(struct demod_state* s);

/**
 * @brief Tear down worker threads created by `demod_mt_init`.
 *
 * @param s Demodulator state used as a key for the worker context.
 * @note Safe no-op if the pool was never enabled/initialized.
 */
void demod_mt_destroy(struct demod_state* s);

/**
 * @brief Post up to two tasks and wait for completion.
 *
 * Runs synchronously in the caller thread when the pool is disabled.
 * @param s Demodulator state key for the worker context.
 * @param f0 Function pointer for the first task (may be NULL).
 * @param a0 Argument for the first task.
 * @param f1 Function pointer for the second task (may be NULL).
 * @param a1 Argument for the second task.
 */
void demod_mt_run_two(struct demod_state* s, void (*f0)(void*), void* a0, void (*f1)(void*), void* a1);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_WORKER_POOL_H */
