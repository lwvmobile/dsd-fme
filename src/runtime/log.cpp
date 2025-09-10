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
 * @brief Runtime logging implementation for environment-independent logging.
 *
 * Implements the low-level write routine used by logging macros to emit
 * messages. Currently forwards to `stderr`. Future enhancements may include
 * runtime level control, timestamps, and file sinks.
 */

#include "runtime/log.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Write a formatted log message to the logging sink.
 *
 * Currently forwards to `stderr`. The `level` parameter is reserved for future
 * runtime gating and may be used to filter messages at runtime.
 *
 * @param level  Log severity level (currently not used for filtering).
 * @param format printf-style format string.
 * @param ...    Variadic arguments corresponding to `format`.
 */
void
dsd_fme_log_write(dsd_fme_log_level_t level, const char* format, ...) {
    (void)level; /* Currently unused, but available for future runtime gating */

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
