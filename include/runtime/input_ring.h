/*
 * Input ring buffer for RTL-SDR USB data
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

/* Simple SPSC ring for interleaved I/Q int16_t samples (input path) */
struct input_ring_state {
    int16_t* buffer;
    size_t capacity; /* in int16_t elements */
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    pthread_cond_t ready;
    pthread_mutex_t ready_m;
};

/**
 * Number of samples currently in the input ring.
 */
static inline size_t
input_ring_used(const struct input_ring_state* r) {
    size_t h = r->head.load();
    size_t t = r->tail.load();
    if (h >= t) {
        return h - t;
    }
    return r->capacity - t + h;
}

/**
 * Number of free slots available for writing in the input ring.
 */
static inline size_t
input_ring_free(const struct input_ring_state* r) {
    return (r->capacity - 1) - input_ring_used(r);
}

/**
 * Check if the input ring is empty.
 */
static inline int
input_ring_is_empty(const struct input_ring_state* r) {
    return r->head.load() == r->tail.load();
}

/**
 * Clear the input ring head/tail indices.
 */
static inline void
input_ring_clear(struct input_ring_state* r) {
    r->tail.store(0);
    r->head.store(0);
}

/**
 * Reserve writable regions in the input ring buffer.
 *
 * @param r          Input ring buffer state.
 * @param min_needed Minimum number of samples needed.
 * @param p1         [out] First writable region pointer.
 * @param n1         [out] First writable region length.
 * @param p2         [out] Second writable region pointer or NULL.
 * @param n2         [out] Second writable region length.
 * @return Total writable samples granted across regions.
 */
int input_ring_reserve(struct input_ring_state* r, size_t min_needed, int16_t** p1, size_t* n1, int16_t** p2,
                       size_t* n2);

/**
 * Commit previously reserved writable regions to the input ring.
 *
 * @param r         Input ring buffer state.
 * @param produced  Number of samples produced to commit.
 */
void input_ring_commit(struct input_ring_state* r, size_t produced);

/**
 * Write samples to the input ring, blocking if necessary.
 *
 * @param r     Input ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
void input_ring_write(struct input_ring_state* r, const int16_t* data, size_t count);

/**
 * Read up to max_count samples from the input ring, blocking until available.
 *
 * @param r         Input ring buffer state.
 * @param out       Destination buffer for samples.
 * @param max_count Maximum number of samples to read.
 * @return Number of samples read (>=1), 0 if max_count is 0, or -1 on exit.
 */
int input_ring_read_block(struct input_ring_state* r, int16_t* out, size_t max_count);
