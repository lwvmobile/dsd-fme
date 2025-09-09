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
void demod_mt_run_two(struct demod_state* s,
                      void (*f0)(void*), void* a0,
                      void (*f1)(void*), void* a1);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_WORKER_POOL_H */


