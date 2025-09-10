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
 * @brief UDP-based remote control interface API.
 *
 * Declares the opaque handle and functions to start and stop a background UDP
 * listener that accepts retune commands and invokes a user-supplied callback.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct udp_control;

/**
 * @brief Callback invoked on valid retune command.
 *
 * @param new_frequency_hz Parsed requested center frequency in Hz.
 * @param user_data Opaque pointer passed from `udp_control_start`.
 */
typedef void (*udp_control_retune_cb)(uint32_t new_frequency_hz, void* user_data);

/**
 * @brief Start UDP control thread.
 *
 * Starts a background UDP listener on the specified port. On valid messages,
 * invokes the provided retune callback with the parsed frequency.
 *
 * @param udp_port UDP port to bind and listen on (0 disables/start no-op).
 * @param cb Callback invoked upon receiving a valid retune command.
 * @param user_data Opaque pointer passed to the callback.
 * @return Opaque handle on success; NULL on failure.
 */
struct udp_control* udp_control_start(int udp_port, udp_control_retune_cb cb, void* user_data);

/**
 * @brief Stop UDP control thread and free resources.
 *
 * Closes the socket, joins the worker thread, and releases the handle.
 *
 * @param ctrl Opaque handle returned by `udp_control_start` (safe to pass NULL).
 */
void udp_control_stop(struct udp_control* ctrl);

#ifdef __cplusplus
}
#endif
