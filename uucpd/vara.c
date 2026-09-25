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
#include <time.h>
#include <stdatomic.h>

#include "uucpd.h"
#include "net.h"
#include "call_uucico.h"
#include "vara.h"
#include "serial.h"

// A DISCONNECT we sent, and when: the next DISCONNECTED from the TNC ends that
// session's teardown, not a session that may have started since.  Private to
// uucpd's control threads, so it lives here and not in the shared rhizo_conn.
static atomic_bool disconnect_pending = false;
static atomic_long disconnect_sent = 0;

// Set when the TNC ends a live session (the peer disconnected, or the link
// was lost): the local uucico and uuport may still run and must be stopped.
// When the session ended on its own (uuport or the called uucico exited),
// they are gone already, and a killall would hit the next session's instead.
static atomic_bool kill_session = false;

void *vara_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_to_read;

    while(connector->shutdown == false){

        // check if we are connected, otherwise, wait
        while (connector->connected == false || circular_buf_size(connector->in_buffer) == 0){
            if (connector->shutdown == true){
                goto exit_local;
            }
            sleep(1);
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

        while (connector->buffer_size + bytes_to_read >  MAX_VARA_BUFFER)
            sleep(1);

        if (tcp_write(connector->data_socket, buffer, bytes_to_read) == false)
        {
            fprintf(stderr, "Error in tcp_write(data_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }
		connector->bytes_buffered_tx += bytes_to_read;
		// fprintf(stderr, "bytes_buffered %d\n", connector->bytes_buffered_tx);

        // buffer management hack
        sleep(1);
    }

exit_local:
    fprintf(stderr, "vara_data_worker_thread_tx exit.\n");
    return EXIT_SUCCESS;
}

void *vara_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[MAX_VARA_PACKET_SAFE];

    while(connector->shutdown == false){

        while (connector->connected == false){
            if (connector->shutdown == true){
                goto exit_local;
            }
            connector->bytes_received = 0;
            sleep(1);
        }

        if (tcp_read(connector->data_socket, buffer, 1) == false)
        {
            connector->shutdown = true;
            goto exit_local;
        }

        while (circular_buf_free_size(connector->out_buffer) < 1)
        {
            usleep(100000); // 100ms
        }
        circular_buf_put_range(connector->out_buffer, buffer, 1);
        connector->bytes_received++;
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
                    modem_bytes_received(connector->bytes_received, connector->radio_type);
                }
                if (connector->bytes_transmitted != last_bytes_tx)
                {
                    // fprintf(stderr, "bytes_transmitted %d\n", connector->bytes_transmitted);
                    last_bytes_tx = connector->bytes_transmitted;
                    modem_bytes_transmitted(connector->bytes_transmitted, connector->radio_type);
                }
            }


            // lets not print IMALIVE watchdog
            if (!memcmp(buffer, "IAMALIVE", strlen("IAMALIVE")))
                continue;

            if (!strcmp((char *) buffer, "DISCONNECTED"))
            {
                fprintf(stderr, "TNC: %s\n", buffer);

                // The end of a teardown we started: the session is already
                // cleaned up, and a new uucico may be running and queueing
                // its data.  Only drop what the old peer sent after our
                // DISCONNECT; tearing down again would kill the new session.
                if (disconnect_pending)
                {
                    circular_buf_reset(connector->out_buffer);
                    disconnect_pending = false;
                    connector->connected = false;
                    connected_led_off(connector->serial_fd, connector->radio_type);
                    connector->waiting_for_connection = false;
                    fprintf(stderr, "Disconnect completed.\n");
                    continue;
                }

                // The TNC can report DISCONNECTED more than once for one
                // teardown (Mercury does).  With no link up and no CONNECT
                // of ours in progress there is no session for it to end:
                // acting on it would kill the next session's uucico.
                if (!connector->connected && !connector->waiting_for_connection)
                {
                    fprintf(stderr, "DISCONNECTED with no link up, ignoring.\n");
                    continue;
                }

                last_bytes_rx = 0;
                last_bytes_tx = 0;
                connector->bytes_received = 0;
                connector->bytes_transmitted = 0;
                connector->bytes_buffered_tx = 0;
                modem_bytes_received(connector->bytes_received, connector->radio_type);
                modem_bytes_transmitted(connector->bytes_transmitted, connector->radio_type);

                kill_session = true;
                connector->clean_buffers = true;
                connector->connected = false;
                connected_led_off(connector->serial_fd, connector->radio_type);
                connector->waiting_for_connection = false;
                continue;
            }

            if (!memcmp(buffer, "CONNECTED", strlen("CONNECTED")))
            {
                fprintf(stderr, "TNC: %s\n", buffer);
                // a new link supersedes a teardown the TNC never confirmed
                disconnect_pending = false;
                connector->connected = true;
                connected_led_on(connector->serial_fd, connector->radio_type);
                if (connector->waiting_for_connection == false)
                { // we are receiving a connection... call uucico!
                    bool retval = call_uucico(connector);
                    if (retval == false)
                        fprintf(stderr, "Error calling call_uucico()!\n");
                }
                connector->waiting_for_connection = false;
                continue;
            }

            if (!memcmp(buffer, "BUFFER", strlen("BUFFER")))
            {
                sscanf( (char *) buffer, "BUFFER %d", &connector->buffer_size);
                if (connector->buffer_size == 0)
                {
                    last_bytes_tx = connector->bytes_transmitted;
                    connector->bytes_transmitted += connector->bytes_buffered_tx;
                    connector->bytes_buffered_tx = 0;
                }
                else if (connector->buffer_size < connector->bytes_buffered_tx)
                {
                    last_bytes_tx = connector->bytes_transmitted;
                    connector->bytes_transmitted += connector->bytes_buffered_tx - connector->buffer_size;
                    connector->bytes_buffered_tx -= connector->bytes_buffered_tx - connector->buffer_size;
                }

                fprintf(stderr, "BUFFER: %d\n", connector->buffer_size);
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
                    modem_bitrate(bitrate, connector->radio_type);
                    continue;
                }

                if (!memcmp(buffer, "SN", strlen("SN")))
                {
                    sscanf( (char *) buffer, "SN %f", &snr);
                    fprintf(stderr, "SN %.1f\n", snr);
                    modem_snr(snr * 10, connector->radio_type);
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
            if (connector->connected == true && !disconnect_pending)
            {
                connector->send_break = false;
                sleep(1);
                memset(buffer,0,sizeof(buffer));
                sprintf(buffer,"DISCONNECT\r");
                // sprintf(buffer,"ABORT\r"); // shouldn't we use abort here?
                ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));
                fprintf(stderr, "SENDING DISCONNECT\n");
                disconnect_sent = (long) time(NULL);
                disconnect_pending = true;
            }
            usleep(1200000); // sleep for threads finish their jobs (more than 1s here)

            // Only when the TNC ended a live session: a uucico still running
            // then belongs to it (it holds the port lock, so no other can
            // have started).  A session that ended on its own has nothing
            // left, and a killall would take the next session's uucico,
            // which a gateway may start within a second.
            if (kill_session)
            {
                kill_session = false;
                system("killall uucico");
                system("killall uuport");
            }

            fprintf(stderr, "Connection closed. Cleaning internal buffers.\n");
            circular_buf_reset(connector->in_buffer);
            circular_buf_reset(connector->out_buffer);

            // Do not wait here for the TNC's DISCONNECTED: on HF its teardown
            // can take tens of seconds, and a uucico started meanwhile must
            // be able to queue its data (uuport stops while clean_buffers is
            // set).  The CONNECT for it goes out once the old link is down.
            connector->clean_buffers = false;
			connector->buffer_size = 0;
        }

        // A TNC that never confirms our DISCONNECT must not hold the link.
        if (disconnect_pending &&
            (long) time(NULL) - disconnect_sent > 90)
        {
            fprintf(stderr, "No DISCONNECTED from the TNC in 90 s, taking the link as down.\n");
            circular_buf_reset(connector->out_buffer);
            disconnect_pending = false;
            connector->connected = false;
            connector->waiting_for_connection = false;
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

static const char *serial_baudrate_for_radio_type(int radio_type)
{
    if (radio_type == RADIO_TYPE_ICOM)
        return "19200";

    if (radio_type == RADIO_TYPE_ICOM_7300)
        return "115200";

    if (radio_type == RADIO_TYPE_UBITX)
        return "38400";

    return NULL;
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

        const char *serial_baudrate = serial_baudrate_for_radio_type(connector->radio_type);

        if (serial_baudrate)
            set_fixed_baudrate(serial_baudrate, connector->serial_fd);
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
