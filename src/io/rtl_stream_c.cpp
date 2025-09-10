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
 * @brief C shim over the C++ RTL-SDR orchestrator for use by C code.
 *
 * Exposes a minimal C API that mirrors lifecycle, tuning, and I/O operations
 * of RtlSdrOrchestrator. Intended to allow incremental migration from the
 * legacy C control paths while preserving behavior.
 */

#include <new>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "dsd.h"
#include "io/rtl_stream_c.h"
// Local forward declarations for legacy helpers used under the hood
void dsd_rtl_stream_clear_output(void);
long dsd_rtl_stream_return_pwr(void);
}

#include "io/rtl_stream.h"

struct RtlSdrContext {
    RtlSdrOrchestrator* stream;
};

/**
 * @brief Create a new RTL-SDR stream context from options.
 *
 * @param opts Decoder options snapshot used to configure the stream. Must not be NULL.
 * @param out_ctx [out] On success, receives an opaque context pointer.
 * @return 0 on success; otherwise <0 on error.
 */
extern "C" int
rtl_stream_create(const dsd_opts* opts, RtlSdrContext** out_ctx) {
    if (!out_ctx || !opts) {
        return -1;
    }
    *out_ctx = (RtlSdrContext*)calloc(1, sizeof(RtlSdrContext));
    if (!*out_ctx) {
        return -1;
    }
    (*out_ctx)->stream = new (std::nothrow) RtlSdrOrchestrator(*opts);
    if (!(*out_ctx)->stream) {
        free(*out_ctx);
        *out_ctx = NULL;
        return -1;
    }
    return 0;
}

/**
 * @brief Start the stream threads and device I/O.
 *
 * @param ctx Stream context created by rtl_stream_create().
 * @return 0 on success; otherwise <0 on error.
 */
extern "C" int
rtl_stream_start(RtlSdrContext* ctx) {
    if (!ctx || !ctx->stream) {
        return -1;
    }
    return ctx->stream->start();
}

/**
 * @brief Stop the stream and cleanup resources associated with the run.
 *
 * Safe to call multiple times; subsequent calls are no-ops.
 *
 * @param ctx Stream context created by rtl_stream_create().
 * @return 0 on success; otherwise <0 on error.
 */
extern "C" int
rtl_stream_stop(RtlSdrContext* ctx) {
    if (!ctx || !ctx->stream) {
        return -1;
    }
    return ctx->stream->stop();
}

/**
 * @brief Destroy the stream context and free all associated resources.
 *
 * If the stream is running, it is stopped before destruction.
 *
 * @param ctx Stream context to destroy. May be NULL.
 * @return 0 always.
 */
extern "C" int
rtl_stream_destroy(RtlSdrContext* ctx) {
    if (!ctx) {
        return 0;
    }
    if (ctx->stream) {
        ctx->stream->stop();
        delete ctx->stream;
        ctx->stream = NULL;
    }
    free(ctx);
    return 0;
}

/**
 * @brief Tune to a new center frequency.
 *
 * @param ctx Stream context.
 * @param center_freq_hz New center frequency in Hz.
 * @return 0 on success; otherwise <0 on error.
 */
extern "C" int
rtl_stream_tune(RtlSdrContext* ctx, uint32_t center_freq_hz) {
    if (!ctx || !ctx->stream) {
        return -1;
    }
    return ctx->stream->tune(center_freq_hz);
}

/**
 * @brief Read up to `count` interleaved audio samples into `out`.
 *
 * @param ctx Stream context.
 * @param out Destination buffer for samples. Must not be NULL.
 * @param count Maximum number of samples to read.
 * @param out_got [out] Set to the number of samples actually read.
 * @return 0 on success; otherwise <0 on error (e.g., shutdown).
 */
extern "C" int
rtl_stream_read(RtlSdrContext* ctx, int16_t* out, size_t count, int* out_got) {
    if (!ctx || !ctx->stream || !out || !out_got) {
        return -1;
    }
    return ctx->stream->read(out, count, *out_got);
}

/**
 * @brief Get the current output sample rate in Hz.
 *
 * @param ctx Stream context.
 * @return Output sample rate in Hz; returns 0 if `ctx` is invalid.
 */
extern "C" uint32_t
rtl_stream_output_rate(const RtlSdrContext* ctx) {
    if (!ctx || !ctx->stream) {
        return 0U;
    }
    return ctx->stream->output_rate();
}

/**
 * @brief Clear the output ring buffer and wake any waiting producer.
 *
 * Mirrors the legacy behavior. The `ctx` parameter is currently ignored.
 *
 * @param ctx Stream context (unused).
 */
extern "C" void
rtl_stream_clear_output(RtlSdrContext* /*ctx*/) {
    // Delegate to legacy function to keep behavior identical
    dsd_rtl_stream_clear_output();
}

/**
 * @brief Return mean power approximation (RMS^2 proxy) for soft squelch.
 *
 * The computation uses a small fixed sample window for efficiency and mirrors
 * the legacy implementation.
 *
 * @param ctx Stream context (unused).
 * @return Mean power value (approximate RMS squared).
 */
extern "C" long
rtl_stream_return_pwr(const RtlSdrContext* /*ctx*/) {
    return dsd_rtl_stream_return_pwr();
}
