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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "dsd.h"

/**
 * @brief RAII-class orchestrator for RTL-SDR streaming pipeline.
 *
 * Wraps the existing C orchestration with a safer lifecycle:
 * constructor stores options, start() initializes and launches threads,
 * stop() tears everything down, and the destructor auto-stops when needed.
 * This maintains current behavior while enabling a cleaner API surface.
 */
class RtlSdrOrchestrator {
  public:
    /**
     * @brief Construct a stream with an option snapshot.
     * @param opts @ref dsd_opts used to configure the stream. Copied internally.
     */
    explicit RtlSdrOrchestrator(const dsd_opts& opts);

    /**
     * @brief Destructor. Ensures stop() is called.
     */
    ~RtlSdrOrchestrator();

    /**
     * @brief Initialize and start the stream threads and device async I/O.
     * @return 0 on success, <0 on error.
     */
    int start();

    /**
     * @brief Stop threads and cleanup resources. Safe to call multiple times.
     * @return 0 on success.
     */
    int stop();

    /**
     * @brief Tune to a new center frequency in Hz.
     * @param center_freq_hz Frequency in Hz.
     * @return 0 on success, <0 on error.
     */
    int tune(uint32_t center_freq_hz);

    /**
     * @brief Read up to count audio samples.
     * @param out Destination buffer.
     * @param count Max samples to read.
     * @param out_got [out] Number of samples read.
     * @return 0 on success, <0 on error (e.g., shutdown).
     */
    int read(int16_t* out, size_t count, int& out_got);

    /**
     * @brief Current output sample rate in Hz.
     */
    unsigned int output_rate() const;

    /**
     * @brief Whether the last operation succeeded.
     */
    bool
    ok() const {
        return last_error_code_ == 0;
    }

    /**
     * @brief Error code from the last failing operation (if any).
     */
    int
    last_error_code() const {
        return last_error_code_;
    }

  private:
    // Non-copyable to avoid accidental shared lifecycle
    RtlSdrOrchestrator(const RtlSdrOrchestrator&) = delete;
    RtlSdrOrchestrator& operator=(const RtlSdrOrchestrator&) = delete;

    // Mutable snapshot of options passed into C API
    dsd_opts* opts_;
    bool started_;
    int last_error_code_;
};
