/*
 * UDP Control Interface
 * Copyright (C) 2025
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct udp_control;

typedef void (*udp_control_retune_cb)(uint32_t new_frequency_hz, void* user_data);

/**
 * Start UDP control thread listening on udp_port. On valid messages, invokes cb.
 * Returns opaque handle or NULL on failure.
 */
struct udp_control* udp_control_start(int udp_port, udp_control_retune_cb cb, void* user_data);

/**
 * Stop UDP control thread, close socket, and free resources. Safe to call with NULL.
 */
void udp_control_stop(struct udp_control* ctrl);

#ifdef __cplusplus
}
#endif
