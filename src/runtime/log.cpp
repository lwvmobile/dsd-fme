/*
 * Runtime Logging Implementation
 *
 * This file implements the logging interface for DSD-FME runtime components,
 * providing structured logging with different severity levels for debugging
 * and monitoring the demodulation pipeline and I/O operations.
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

#include "runtime/log.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * Internal logging function - currently just forwards to stderr.
 * Future: could add runtime level control, timestamps, file logging, etc.
 *
 * @param level  Log severity level (currently unused).
 * @param format printf-style format string.
 * @param ...    Variadic arguments for the format string.
 */
void
dsd_fme_log_write(dsd_fme_log_level_t level, const char* format, ...) {
    (void)level; /* Currently unused, but available for future runtime gating */

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
