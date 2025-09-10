/*
 * RAII orchestrator for RTL-SDR stream lifecycle and control
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

#include "io/rtl_stream.h"
#include <string.h>

extern "C" {
#include "dsd.h"
// Local forward declarations for legacy functions now hidden from public headers
int dsd_rtl_stream_open(dsd_opts* opts);
void dsd_rtl_stream_close(void);
int dsd_rtl_stream_read(int16_t* out, size_t count, dsd_opts* opts, dsd_state* state);
int dsd_rtl_stream_tune(dsd_opts* opts, long int frequency);
unsigned int dsd_rtl_stream_output_rate(void);
}

namespace {
static dsd_opts*
copy_opts(const dsd_opts* src) {
    if (!src) {
        return nullptr;
    }
    dsd_opts* dst = (dsd_opts*)malloc(sizeof(dsd_opts));
    if (!dst) {
        return nullptr;
    }
    memcpy(dst, src, sizeof(dsd_opts));
    return dst;
}
} // namespace

RtlSdrOrchestrator::RtlSdrOrchestrator(const dsd_opts& opts)
    : opts_(copy_opts(&opts)), started_(false), last_error_code_(0) {}

RtlSdrOrchestrator::~RtlSdrOrchestrator() {
    stop();
    if (opts_) {
        free(opts_);
        opts_ = nullptr;
    }
}

int
RtlSdrOrchestrator::start() {
    if (started_) {
        return 0;
    }
    if (!opts_) {
        last_error_code_ = -1;
        return last_error_code_;
    }
    int r = dsd_rtl_stream_open(opts_);
    if (r < 0) {
        last_error_code_ = r;
        return r;
    }
    started_ = true;
    last_error_code_ = 0;
    return 0;
}

int
RtlSdrOrchestrator::stop() {
    if (!started_) {
        return 0;
    }
    dsd_rtl_stream_close();
    started_ = false;
    last_error_code_ = 0;
    return 0;
}

int
RtlSdrOrchestrator::tune(uint32_t center_freq_hz) {
    if (!started_) {
        last_error_code_ = -1;
        return last_error_code_;
    }
    if (!opts_) {
        last_error_code_ = -2;
        return last_error_code_;
    }
    dsd_rtl_stream_tune(opts_, (long int)center_freq_hz);
    last_error_code_ = 0;
    return 0;
}

int
RtlSdrOrchestrator::read(int16_t* out, size_t count, int& out_got) {
    if (!started_) {
        last_error_code_ = -1;
        return last_error_code_;
    }
    if (!opts_) {
        last_error_code_ = -2;
        return last_error_code_;
    }
    int got = dsd_rtl_stream_read(out, count, opts_, (dsd_state*)nullptr);
    if (got < 0) {
        last_error_code_ = got;
        return got;
    }
    out_got = got;
    last_error_code_ = 0;
    return 0;
}

unsigned int
RtlSdrOrchestrator::output_rate() const {
    return dsd_rtl_stream_output_rate();
}
