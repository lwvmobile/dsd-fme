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

#include <cstring>
#include <time.h>
#include "runtime/input_ring.h"

extern int exitflag; // defined in rtl_sdr_fm.cpp

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
int
input_ring_reserve(struct input_ring_state* r, size_t min_needed, int16_t** p1, size_t* n1, int16_t** p2, size_t* n2) {
    size_t free_sp = input_ring_free(r);
    /* Producer must never advance consumer tail; if full, grant nothing */
    /* Provide up to min(free_sp, min_needed) across at most two regions */
    size_t grant = (min_needed < free_sp) ? min_needed : free_sp;
    size_t h = r->head.load();

    *p1 = NULL;
    *n1 = 0;
    *p2 = NULL;
    *n2 = 0;

    if (grant == 0) {
        return 0;
    }

    /* First region: from head to end of buffer */
    *p1 = r->buffer + h;
    size_t to_end = r->capacity - h;
    if (to_end >= grant) {
        *n1 = grant;
        return (int)grant;
    }
    *n1 = to_end;

    /* Second region: wrap around to beginning */
    if (grant > to_end) {
        *p2 = r->buffer;
        *n2 = grant - to_end;
    }

    return (int)grant;
}

/**
 * Commit previously reserved writable regions to the input ring.
 *
 * @param r         Input ring buffer state.
 * @param produced  Number of samples produced to commit.
 */
void
input_ring_commit(struct input_ring_state* r, size_t produced) {
    if (produced == 0) {
        return;
    }
    int need_signal = input_ring_is_empty(r);
    size_t h = r->head.load();
    h += produced;
    if (h >= r->capacity) {
        h -= r->capacity;
    }
    r->head.store(h);
    if (need_signal) {
        pthread_mutex_lock(&r->ready_m);
        pthread_cond_signal(&r->ready);
        pthread_mutex_unlock(&r->ready_m);
    }
}

/**
 * Write samples to the input ring, blocking if necessary.
 *
 * @param r     Input ring buffer state.
 * @param data  Source samples to write.
 * @param count Number of samples to write.
 */
void
input_ring_write(struct input_ring_state* r, const int16_t* data, size_t count) {
    int need_signal = input_ring_is_empty(r);
    while (count > 0 && !exitflag) {
        size_t free_sp = input_ring_free(r);
        if (free_sp == 0) {
            /* Ring full: to avoid racing the consumer, drop remainder */
            break;
        }
        size_t write_now = (count < free_sp) ? count : free_sp;
        size_t h = r->head.load();

        /* First region: from head to end of buffer */
        size_t to_end = r->capacity - h;
        if (to_end >= write_now) {
            memcpy(r->buffer + h, data, write_now * sizeof(int16_t));
            h += write_now;
            if (h >= r->capacity) {
                h = 0;
            }
            r->head.store(h);
            data += write_now;
            count -= write_now;
            continue;
        }

        /* Handle wrap-around case */
        if (to_end > 0) {
            memcpy(r->buffer + h, data, to_end * sizeof(int16_t));
            data += to_end;
            count -= to_end;
        }
        memcpy(r->buffer, data, count * sizeof(int16_t));
        h = count;
        r->head.store(h);
        data += count;
        count = 0;
    }
    if (need_signal) {
        pthread_mutex_lock(&r->ready_m);
        pthread_cond_signal(&r->ready);
        pthread_mutex_unlock(&r->ready_m);
    }
}

/**
 * Read up to max_count samples from the input ring, blocking until available.
 *
 * @param r         Input ring buffer state.
 * @param out       Destination buffer for samples.
 * @param max_count Maximum number of samples to read.
 * @return Number of samples read (>=1), 0 if max_count is 0, or -1 on exit.
 */
int
input_ring_read_block(struct input_ring_state* r, int16_t* out, size_t max_count) {
    if (max_count == 0) {
        return 0;
    }
    while (input_ring_is_empty(r)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 10L * 1000000L; /* 10ms */
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_mutex_lock(&r->ready_m);
        int ret = pthread_cond_timedwait(&r->ready, &r->ready_m, &ts);
        pthread_mutex_unlock(&r->ready_m);
        if (ret != 0) {
            if (exitflag) {
                return -1;
            }
            /* Timeout: check again */
            continue;
        }
    }

    size_t available = input_ring_used(r);
    size_t read_now = (max_count < available) ? max_count : available;
    size_t t = r->tail.load();
    size_t first = r->capacity - t;
    if (first >= read_now) {
        memcpy(out, r->buffer + t, read_now * sizeof(int16_t));
        t += read_now;
        if (t >= r->capacity) {
            t = 0;
        }
    } else {
        memcpy(out, r->buffer + t, first * sizeof(int16_t));
        memcpy(out + first, r->buffer, (read_now - first) * sizeof(int16_t));
        t = read_now - first;
    }
    r->tail.store(t);

    return (int)read_now;
}
