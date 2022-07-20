/* Rhizo-uuhf: Tools to integrate HF TNCs to UUCP
 * Copyright (C) 2019-2021 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

/**
 * @file uuardopd.h
 * @author Rafael Diniz
 * @date 10 Jul 2019
 * @brief UUCP ARDOP daemon header
 *
 * UUCP ARDOP daemon
 *
 */

#ifndef HAVE_UUARDOPD_H__
#define HAVE_UUARDOPD_H__

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

#include "circular_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UUCP_CONFIG "/etc/uucp/config"

// 30s
#define TIMEOUT_DEFAULT 30

#define INTERNAL_BUFFER_SIZE 1048576  // (2 ^ 20)

#define BUFFER_SIZE 4096

typedef struct{
// modem related data
    char call_sign[32];
    char remote_call_sign[32];
    int tcp_base_port;
    char ip_address[32];
    char modem_type[32];
    int radio_type;
    bool serial_keying;
    int serial_fd;
    char serial_path[1024];
    bool ofdm_mode;
    uint16_t vara_mode;
    bool ask_login;
    int timeout;
    int data_socket;
    int control_socket;

    // pipe fd's for talking to exec'ed uucico (recv call)
    int pipefd1[2];
    int pipefd2[2];

// state variables (TODO: use FSM!)
// C11 atomic is used here instead of a more pedantic code with mutexes and so on... 
    atomic_bool shutdown;
    atomic_bool connected;
    atomic_bool waiting_for_connection;
    atomic_bool clean_buffers;
    atomic_bool uucico_active;
    atomic_bool send_break;

    atomic_int session_counter_read;
    atomic_int session_counter_write;

    // internal ardop buffer size
    atomic_int buffer_size;

// uuardopd private buffers
    cbuf_handle_t in_buffer;
    cbuf_handle_t out_buffer;
//uuport private buffers
    cbuf_handle_t in_buffer_p;
    cbuf_handle_t out_buffer_p;
} rhizo_conn;

// from ubitx_controller.h - TODO: share the same header across projects!
typedef struct{

    uint8_t service_command[5];
    pthread_mutex_t cmd_mutex;
    pthread_cond_t cmd_condition;

    pthread_mutex_t response_mutex;

    uint8_t response_service[5];
    atomic_bool response_service_type;
    atomic_bool response_available;

    // special response for ptt
    atomic_uchar ptt_last_response;

    // Protection alert ON!
    atomic_bool protection_alert;

    int radio_fd;

} controller_conn;


#ifdef __cplusplus
};
#endif

#endif /* HAVE_UUARDOPD_H__ */
