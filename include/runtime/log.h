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

#ifndef DSD_FME_LOG_H
#define DSD_FME_LOG_H

/**
 * @file
 * @brief Runtime logging interface used across DSD-FME components.
 *
 * Declares log severity levels, the core logging write routine, and convenience
 * macros. The implementation currently forwards messages to `stderr`.
 */

/**
 * @brief Log severity levels for runtime logging.
 */
typedef enum { LOG_LEVEL_ERROR = 0, LOG_LEVEL_WARN = 1, LOG_LEVEL_INFO = 2, LOG_LEVEL_DEBUG = 3 } dsd_fme_log_level_t;

/* Compile-time log level control (default to INFO) */
#ifndef DSD_FME_LOG_LEVEL
#define DSD_FME_LOG_LEVEL LOG_LEVEL_INFO
#endif

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
void dsd_fme_log_write(dsd_fme_log_level_t level, const char* format, ...);

/* Logging macros that map directly to fprintf for initial compatibility */

/* Error messages - always shown */
#define LOG_ERROR(...)                                                                                                 \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
    } while (0)

/* Warning messages - always shown */
#define LOG_WARN(...)                                                                                                  \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
    } while (0)

/* Info messages - always shown */
#define LOG_INFO(...)                                                                                                  \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
    } while (0)

/* Debug messages - compile-time gated */
#if DSD_FME_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...)                                                                                                 \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
    } while (0)
#else
#define LOG_DEBUG(...)                                                                                                 \
    do {                                                                                                               \
        /* Debug logging disabled */                                                                                   \
    } while (0)
#endif

/* Convenience macros for specific message types */

/* For warnings with WARNING: prefix */
#define LOG_WARNING(...)  LOG_WARN("WARNING: " __VA_ARGS__)

/* For notices with NOTICE: prefix */
#define LOG_NOTICE(...)   LOG_INFO("NOTICE: " __VA_ARGS__)

/* For critical errors that may exit */
#define LOG_CRITICAL(...) LOG_ERROR(__VA_ARGS__)

#endif /* DSD_FME_LOG_H */
