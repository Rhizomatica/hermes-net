/* Rhizo-uuardop
 * Copyright (C) 2018-2021 Rhizomatica
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
 * Vara support routines
 */

/**
 * @file vara.c
 * @author Rafael Diniz
 * @date 22 Dec 2020
 * @brief VARA modem support functions
 *
 * All the specific code for supporting VARA.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>

#include "uucpd.h"
#include "net.h"
#include "call_uucico.h"
#include "vara.h"
#include "serial.h"

static void atomic_int_clamp_sub(atomic_int *v, int dec)
{
    if (!v || dec <= 0)
        return;

    int cur = atomic_load_explicit(v, memory_order_relaxed);
    while (cur > 0)
    {
        int next = cur - dec;
        if (next < 0)
            next = 0;
        if (atomic_compare_exchange_weak_explicit(v, &cur, next, memory_order_relaxed, memory_order_relaxed))
            return;
    }
}

static void atomic_int_max(atomic_int *v, int min_val)
{
    if (!v)
        return;

    int cur = atomic_load_explicit(v, memory_order_relaxed);
    while (cur < min_val)
    {
        if (atomic_compare_exchange_weak_explicit(v, &cur, min_val, memory_order_relaxed, memory_order_relaxed))
            return;
    }
}

void *vara_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_to_read;
    const useconds_t poll_us = 20000; // 20ms: keeps latency low without busy spinning

    while(connector->shutdown == false){

        // check if we are connected, otherwise, wait
        while (connector->connected == false || circular_buf_size(connector->in_buffer) == 0){
            if (connector->shutdown == true){
                goto exit_local;
            }
            usleep(poll_us);
        }

    check_data_again:
        bytes_to_read = circular_buf_size(connector->in_buffer);
        if (bytes_to_read > BUFFER_SIZE)
            bytes_to_read = BUFFER_SIZE;
        if (bytes_to_read == 0)
        {
            usleep(50000); // 50ms
            goto check_data_again;
        }

        circular_buf_get_range(connector->in_buffer, buffer, bytes_to_read);

        // fprintf(stderr, "vara_data_worker_thread_tx: Read %d for sending to VARA\n", bytes_to_read);

        for (;;)
        {
            /* bytes_buffered_tx is our conservative "bytes pending" estimate (socket + modem backlog). */
            int pending = atomic_load_explicit(&connector->bytes_buffered_tx, memory_order_relaxed);
            if (pending + bytes_to_read <= MAX_VARA_BUFFER)
                break;
            if (connector->shutdown == true)
                goto exit_local;
            usleep(poll_us);
        }

        if (tcp_write(connector->data_socket, buffer, bytes_to_read) == false)
        {
            fprintf(stderr, "Error in tcp_write(data_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }
        atomic_fetch_add_explicit(&connector->bytes_buffered_tx, bytes_to_read, memory_order_relaxed);
		// fprintf(stderr, "bytes_buffered %d\n", connector->bytes_buffered_tx);

        // Small yield to keep control RX (BUFFER updates) responsive while avoiding 1Hz throttling.
        usleep(1000);
    }

exit_local:
    fprintf(stderr, "vara_data_worker_thread_tx exit.\n");
    return EXIT_SUCCESS;
}

void *vara_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[MAX_VARA_PACKET_SAFE];
    const useconds_t poll_us = 20000; // 20ms

    while(connector->shutdown == false){

        while (connector->connected == false){
            if (connector->shutdown == true){
                goto exit_local;
            }
            atomic_store_explicit(&connector->bytes_received, 0, memory_order_relaxed);
            usleep(poll_us);
        }

        ssize_t n = recv(connector->data_socket, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
                continue;
            connector->shutdown = true;
            goto exit_local;
        }

        while (circular_buf_free_size(connector->out_buffer) < (size_t)n)
        {
            usleep(poll_us);
        }
        circular_buf_put_range(connector->out_buffer, buffer, (size_t)n);
        atomic_fetch_add_explicit(&connector->bytes_received, (int)n, memory_order_relaxed);
    }

exit_local:
    fprintf(stderr, "vara_data_worker_thread_rx exit.\n");
    return EXIT_SUCCESS;
}

void *vara_control_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t rcv_byte;
    uint8_t buffer[1024];
    int bitrate_index = 0;
    int bitrate = 0;
    float snr = 0.0;
    int counter = 0;
    atomic_int last_bytes_rx = 0, last_bytes_tx = 0;
    int last_buffer_report = 0;
    bool new_cmd = false;

    while(connector->shutdown == false)
    {
        if (tcp_read(connector->control_socket, &rcv_byte, 1) == false)
        {
            fprintf(stderr, "Error in tcp_read(control_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }

        if (rcv_byte == '\r'){
            buffer[counter] = 0;
            counter = 0;
            new_cmd = true;
        }
        else{
            buffer[counter] = rcv_byte;
            counter++;
            new_cmd = false;
        }

        if (new_cmd)
        {
            // fprintf(stderr, "cmd %s\n", buffer);

            if (connector->radio_type == RADIO_TYPE_SHM)
            {
				// fprintf(stderr, "tx: %d buffered: %d last: %d\n", connector->bytes_transmitted, connector->bytes_buffered_tx, last_bytes_tx);
                if (connector->bytes_received != last_bytes_rx)
                {
                    // fprintf(stderr, "bytes_received %d\n", connector->bytes_received);
                    last_bytes_rx = connector->bytes_received;
                    modem_bytes_received(connector->bytes_received);
                }
                if (connector->bytes_transmitted != last_bytes_tx)
                {
                    // fprintf(stderr, "bytes_transmitted %d\n", connector->bytes_transmitted);
                    last_bytes_tx = connector->bytes_transmitted;
                    modem_bytes_transmitted(connector->bytes_transmitted);
                }
            }


            // lets not print IMALIVE watchdog
            if (!memcmp(buffer, "IAMALIVE", strlen("IAMALIVE")))
                continue;

            if (!strcmp((char *) buffer, "DISCONNECTED"))
            {
                fprintf(stderr, "TNC: %s\n", buffer);

                last_bytes_tx = 0;
                last_bytes_tx = 0;
                connector->bytes_received = 0;
                connector->bytes_transmitted = 0;
                connector->bytes_buffered_tx = 0;
                last_buffer_report = 0;
                modem_bytes_received(connector->bytes_received);
                modem_bytes_transmitted(connector->bytes_transmitted);

                connector->clean_buffers = true;
                connector->connected = false;
                connected_led_off(connector->serial_fd, connector->radio_type);
                connector->waiting_for_connection = false;
                continue;
            }

            if (!memcmp(buffer, "CONNECTED", strlen("CONNECTED")))
            {
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = true;
                connected_led_on(connector->serial_fd, connector->radio_type);
                if (connector->waiting_for_connection == false)
                { // we are receiving a connection... call uucico!
                    bool retval = call_uucico(connector);
                    if (retval == false)
                        fprintf(stderr, "Error calling call_uucico()!\n");
                }
                connector->waiting_for_connection = false;
                last_buffer_report = 0;
                continue;
            }

            if (!memcmp(buffer, "BUFFER", strlen("BUFFER")))
            {
                int new_buf = 0;
                if (sscanf((char *)buffer, "BUFFER %d", &new_buf) != 1)
                    continue;

                atomic_store_explicit(&connector->buffer_size, new_buf, memory_order_relaxed);

                int drained = 0;
                if (new_buf < last_buffer_report)
                {
                    drained = last_buffer_report - new_buf;
                    connector->bytes_transmitted += drained;
                    atomic_int_clamp_sub(&connector->bytes_buffered_tx, drained);
                }
                last_buffer_report = new_buf;

                /* Never let our estimate drop below what the TNC reports. */
                atomic_int_max(&connector->bytes_buffered_tx, new_buf);

                fprintf(stderr, "BUFFER: %d\n", new_buf);
                continue;
            }

            if (connector->serial_keying == true || connector->radio_type == RADIO_TYPE_SHM)
            {
                if (!memcmp(buffer, "PTT ON", strlen("PTT ON")))
                {
                    key_on(connector->serial_fd, connector->radio_type);
                    fprintf(stderr, "%s\n", buffer);
                    continue;
                }
                if(!memcmp(buffer, "PTT OFF", strlen("PTT OFF")))
                {
                    key_off(connector->serial_fd, connector->radio_type);
                    fprintf(stderr, "%s\n", buffer);
                    continue;
                }
            }

            if (connector->radio_type == RADIO_TYPE_SHM)
            {
                if (!memcmp(buffer, "BITRATE", strlen("BITRATE")))
                {
                    sscanf( (char *) buffer, "BITRATE (%d) %d", &bitrate_index, &bitrate);
                    fprintf(stderr, "BITRATE (%d) %d bps\n", bitrate_index, bitrate);
                    modem_bitrate(bitrate);
                    continue;
                }

                if (!memcmp(buffer, "SN", strlen("SN")))
                {
                    sscanf( (char *) buffer, "SN %f", &snr);
                    fprintf(stderr, "SN %.1f\n", snr);
                    modem_snr(snr * 10);
                    continue;
                }
            }
        }
    }

exit_local:
    fprintf(stderr, "vara_control_worker_thread_rx exit.\n");
    return EXIT_SUCCESS;
}

void *vara_control_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    char buffer[1024];
    bool ret = true;

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"LISTEN ON\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"PUBLIC OFF\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    // sprintf(buffer,"COMPRESSION FILES\r");
    // sprintf(buffer,"COMPRESSION TEXT\r");
    sprintf(buffer,"COMPRESSION OFF\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    uint16_t vara_mode = connector->vara_mode & 0x3fff;
    bool vara_p2p_mode = (connector->vara_mode & 0x4000) ? true : false;

    fprintf(stderr, "Vara mode: %d%s\n", vara_mode, vara_p2p_mode?"p":"");

    memset(buffer,0,sizeof(buffer));
    sprintf(buffer,"BW%u\r", vara_mode);
    // strcpy(buffer,"BW2300\r");
    // strcpy(buffer,"BW500\r");
    // strcpy(buffer,"BW2750\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    if (vara_p2p_mode)
    {
        memset(buffer,0,sizeof(buffer));
        strcpy(buffer,"P2P SESSION\r");
        ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));
    }

    // check lost tcp connection
    if (ret == false)
    {
        fprintf(stderr, "Error in tcp_write(control_socket)\n");
        connector->shutdown = true;
        goto exit_local;
    }

    // 1Hz function
    while(connector->shutdown == false){

        ret = true;

        if (connector->clean_buffers == true)
        {
            if (connector->connected == true)
            {
                connector->send_break = false;
                sleep(1);
                memset(buffer,0,sizeof(buffer));
                sprintf(buffer,"DISCONNECT\r");
                // sprintf(buffer,"ABORT\r"); // shouldn't we use abort here?
                ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));
                fprintf(stderr, "SENDING DISCONNECT\n");
            }
            usleep(1200000); // sleep for threads finish their jobs (more than 1s here)

//            fprintf(stderr, "Killing uucico.\n");
            system("killall uucico");

//            fprintf(stderr, "Killing uuport.\n");
            system("killall uuport");

//            fprintf(stderr, "Connection closed - Cleaning internal buffers.\n");
            circular_buf_reset(connector->in_buffer);
            circular_buf_reset(connector->out_buffer);

            while (connector->connected == true)
                usleep(100000);

            usleep(1200000); // sleep for threads finish their jobs (more than 1s here)

            fprintf(stderr, "Connection closed. Cleaning internal buffers.\n");
            circular_buf_reset(connector->in_buffer);
            circular_buf_reset(connector->out_buffer);

            connector->clean_buffers = false;
			connector->buffer_size = 0;
        }

        // Logic to start a connection
        // condition for connection: no connection AND something to transmitt AND we did not issue a CONNECT recently
        if (connector->connected == false &&
            circular_buf_size(connector->in_buffer) > 0 &&
            !connector->waiting_for_connection){

            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"CONNECT %s %s\r", connector->call_sign,
                    connector->remote_call_sign);
            ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));

            fprintf(stderr, "CONNECTING... %s\n", buffer);

            connector->waiting_for_connection = true;
        }

        // check lost tcp connection
        if (ret == false)
        {
            fprintf(stderr, "Error in tcp_write(control_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }

        sleep(1); // 1Hz function
    }

exit_local:
    fprintf(stderr, "vara_control_worker_thread_tx exit.\n");
    return EXIT_SUCCESS;
}

bool initialize_modem_vara(rhizo_conn *connector)
{
    bool ret;

    connector->buffer_size = 0;
    connector->bytes_received = 0;
    connector->bytes_transmitted = 0;
    connector->bytes_buffered_tx = 0;

try_connect_again:
    ret = true;
    ret &= tcp_connect(connector->ip_address, connector->tcp_base_port, &connector->control_socket);
    ret &= tcp_connect(connector->ip_address, connector->tcp_base_port+1, &connector->data_socket);

    if (ret == false)
    {
        fprintf(stderr, "Connection to TNC failure. Trying again\n");
        close(connector->control_socket);
        close(connector->data_socket);
        sleep(1);
        goto try_connect_again;
        // connector->shutdown = true;
        // return false;
    }

    if (connector->serial_keying == true)
    {
        connector->serial_fd = open_serial_port(connector->serial_path);

        if (connector->serial_fd == -1)
        {
            fprintf(stderr, "Could not open serial device.\n");
            return false;
        }

        if (connector->radio_type == RADIO_TYPE_ICOM)
            set_fixed_baudrate("19200", connector->serial_fd);

        if (connector->radio_type == RADIO_TYPE_UBITX)
            set_fixed_baudrate("38400", connector->serial_fd);
    }

    sys_led_on(connector->serial_fd, connector->radio_type);

    // we start our control thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, vara_control_worker_thread_rx, (void *) connector);

    // we start our control tx thread
    pthread_t tid2;
    pthread_create(&tid2, NULL, vara_control_worker_thread_tx, (void *) connector);

    pthread_t tid3;
    pthread_create(&tid3, NULL, vara_data_worker_thread_tx, (void *) connector);

    pthread_t tid4;
    pthread_create(&tid4, NULL, vara_data_worker_thread_rx, (void *) connector);

//    pthread_t tid5;
//    pthread_create(&tid5, NULL, connection_timeout_thread, (void *) connector);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);
//    pthread_join(tid5, NULL);

    return true;
}
