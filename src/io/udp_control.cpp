/*
 * UDP Control Interface Implementation
 *
 * This file implements the UDP-based remote control interface for DSD-FME,
 * providing network socket communication for runtime parameter configuration
 * and demodulation status monitoring. It enables external control and
 * monitoring of the demodulation process.
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

#include "io/udp_control.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "runtime/log.h"

struct udp_control {
    int port;
    int sockfd;
    pthread_t thread;
    udp_control_retune_cb cb;
    void* user_data;
    volatile int stop_flag;
};

/**
 * Convert 4-byte little-endian payload (following a leading command byte) to
 * a 32-bit unsigned integer.
 *
 * @param buf Pointer to 5-byte buffer where buf[0] is a command and buf[1..4]
 *            encode the value as little-endian.
 * @return Parsed 32-bit value.
 */
static unsigned int
udp_chars_to_int(unsigned char* buf) {
    int i;
    unsigned int val = 0;
    for (i = 1; i < 5; i++) {
        val = val | ((buf[i]) << ((i - 1) * 8));
    }
    return val;
}

/**
 * UDP control thread entry. Binds to INADDR_ANY:udp_port and listens for
 * 5-byte messages. When a valid tune command is received, invokes the
 * registered callback with the new frequency.
 *
 * @param arg Pointer to udp_control.
 * @return NULL on exit.
 */
static void*
udp_thread_fn(void* arg) {
    udp_control* ctrl = (udp_control*)arg;
    int n;
    unsigned char buffer[5];
    struct sockaddr_in serv_addr;

    ctrl->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctrl->sockfd < 0) {
        perror("ERROR opening socket");
        return NULL;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t)ctrl->port);

    if (bind(ctrl->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        close(ctrl->sockfd);
        ctrl->sockfd = -1;
        return NULL;
    }

    memset(buffer, 0, sizeof(buffer));
    LOG_INFO("Main socket started! :-) Tuning enabled on UDP/%d \n", ctrl->port);

    while (!ctrl->stop_flag && (n = (int)read(ctrl->sockfd, buffer, 5)) > 0) {
        if (n == 5 && buffer[0] == 0) {
            unsigned int new_freq = udp_chars_to_int(buffer);
            if (ctrl->cb) {
                ctrl->cb(new_freq, ctrl->user_data);
            }
            LOG_INFO("\nTuning to: %u [Hz] \n", new_freq);
        }
    }
    if (!ctrl->stop_flag && n < 0) {
        perror("ERROR on read");
    }

    if (ctrl->sockfd >= 0) {
        close(ctrl->sockfd);
        ctrl->sockfd = -1;
    }
    return NULL;
}

/**
 * Start UDP control thread listening on udp_port. On valid messages, invokes cb.
 * Returns opaque handle or NULL on failure.
 */
extern "C" udp_control*
udp_control_start(int udp_port, udp_control_retune_cb cb, void* user_data) {
    if (udp_port == 0) {
        return NULL;
    }
    udp_control* ctrl = (udp_control*)malloc(sizeof(udp_control));
    if (!ctrl) {
        return NULL;
    }
    ctrl->port = udp_port;
    ctrl->sockfd = -1;
    ctrl->cb = cb;
    ctrl->user_data = user_data;
    ctrl->stop_flag = 0;
    int rc = pthread_create(&ctrl->thread, NULL, udp_thread_fn, ctrl);
    if (rc != 0) {
        free(ctrl);
        return NULL;
    }
    return ctrl;
}

/**
 * Stop UDP control thread, close socket, and free resources. Safe to call with NULL.
 */
extern "C" void
udp_control_stop(udp_control* ctrl) {
    if (!ctrl) {
        return;
    }
    ctrl->stop_flag = 1;
    if (ctrl->sockfd >= 0) {
        shutdown(ctrl->sockfd, SHUT_RDWR);
    }
    pthread_join(ctrl->thread, NULL);
    if (ctrl->sockfd >= 0) {
        close(ctrl->sockfd);
        ctrl->sockfd = -1;
    }
    free(ctrl);
}
