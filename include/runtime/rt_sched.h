/*
 * Realtime Scheduling Header
 *
 * This header provides utilities for realtime scheduling and CPU affinity
 * management. It enables SCHED_FIFO priority scheduling and core pinning
 * for critical demodulation threads to ensure low-latency audio processing.
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

#ifndef DSD_FME_RT_SCHED_H
#define DSD_FME_RT_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optionally enable realtime scheduling and set CPU affinity for the current
 * thread based on environment variables.
 *
 * When `DSD_FME_RT_SCHED=1`, attempts to switch the calling thread to
 * SCHED_FIFO with a priority derived from `DSD_FME_RT_PRIO_<ROLE>` if present.
 * If `DSD_FME_CPU_<ROLE>` is set to a valid CPU index, pins the thread to that
 * CPU.
 *
 * @param role Optional role label (e.g. "DEMOD", "DONGLE", "USB").
 */
void maybe_set_thread_realtime_and_affinity(const char* role);

#ifdef __cplusplus
}
#endif

#endif /* DSD_FME_RT_SCHED_H */
