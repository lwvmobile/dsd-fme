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
 * @brief Output ring buffer API for demodulated audio samples.
 */
 
#pragma once

#include <atomic>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#define safe_cond_signal(n, m)                                                                                         \
    pthread_mutex_lock(m);                                                                                             \
    pthread_cond_signal(n);                                                                                            \
    pthread_mutex_unlock(m)
#define safe_cond_wait(n, m)                                                                                           \
    pthread_mutex_lock(m);                                                                                             \
    pthread_cond_wait(n, m);                                                                                           \
    pthread_mutex_unlock(m)

struct output_state {
    int rate;
    int16_t* buffer;
    size_t capacity;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    pthread_cond_t ready;
    pthread_mutex_t ready_m;
    pthread_cond_t space;
    /* Metrics (Phase 3): track consumer/producer behavior */
    std::atomic<uint64_t> write_timeouts; /* producer waited for space */
    std::atomic<uint64_t> read_timeouts;  /* consumer waited for data */
};

/**
 * @brief Number of queued samples in the output ring.
 *
 * @param o Output ring state.
 * @return Number of queued samples.
 */
static inline size_t
ring_used(const struct output_state* o) {
    /* Atomics policy: head/tail are atomics. We use default sequential
       consistency for simplicity. In an SPSC ring, this could be relaxed
       to acquire/release without changing behavior. */
    size_t h = o->head.load();
    size_t t = o->tail.load();
    if (h >= t) {
        return h - t;
    }
    return o->capacity - t + h;
}

/**
 * @brief Number of writable samples before the ring becomes full.
 *
 * @param o Output ring state.
 * @return Number of writable samples before the ring becomes full.
 */
static inline size_t
ring_free(const struct output_state* o) {
    return (o->capacity - 1) - ring_used(o);
}

/**
 * @brief Check if the output ring is empty.
 *
 * @param o Output ring state.
 * @return Non-zero if empty, zero otherwise.
 */
static inline int
ring_is_empty(const struct output_state* o) {
    /* See atomics note in ring_used() for ordering considerations. */
    return o->head.load() == o->tail.load();
}

/**
 * @brief Clear the output ring head/tail indices.
 *
 * @param o Output ring state to clear.
 */
static inline void
ring_clear(struct output_state* o) {
    /* Clearing indices; with relaxed ordering this would be a release store. */
    o->tail.store(0);
    o->head.store(0);
}

/**
 * @brief Write up to count samples, blocking until space is available.
 *
 * Signals data availability only on an empty-to-non-empty transition.
 *
 * @param o     Output ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
void ring_write(struct output_state* o, const int16_t* data, size_t count);

/**
 * @brief Write up to count samples, blocking until space is available.
 *
 * Does not signal; caller should decide when to signal.
 *
 * @param o     Output ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
void ring_write_no_signal(struct output_state* o, const int16_t* data, size_t count);

/**
 * @brief Write samples with signal on empty-to-non-empty transition.
 *
 * @param o     Output ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
void ring_write_signal_on_empty_transition(struct output_state* o, const int16_t* data, size_t count);

/**
 * @brief Read one sample from the output ring, blocking with timeout until available.
 *
 * @param o    Output ring buffer state.
 * @param out  Destination for one sample.
 * @return 0 on success, -1 on exit.
 */
int ring_read_one(struct output_state* o, int16_t* out);

/**
 * @brief Read up to max_count samples into out.
 *
 * Blocks until at least one sample is available or exit.
 *
 * @param o         Output ring buffer state.
 * @param out       Destination buffer for samples.
 * @param max_count Maximum number of samples to read.
 * @return Number of samples read (>=1) or -1 on exit.
 */
int ring_read_batch(struct output_state* o, int16_t* out, size_t max_count);
