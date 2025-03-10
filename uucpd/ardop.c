/* Rhizo-uuardop: Tools to integrate Ardop to UUCP
 * Copyright (C) 2019 Rhizomatica
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
 * Ardop support routines
 */

/**
 * @file ardop.c
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief Ardop modem support functions
 *
 * All the specific code for supporting Ardop.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "ardop.h"
#include "net.h"
#include "call_uucico.h"
#include "serial.h"

void *ardop_data_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_to_read;
    uint8_t ardop_size[2];
    uint32_t packet_size;

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

        fprintf(stderr, "ardop_data_worker_thread_tx: After read buffer\n");

        packet_size = bytes_to_read;

        uint32_t counter = 0;
        uint32_t tx_size = packet_size;
        while (tx_size != 0){

            while (connector->buffer_size + packet_size >  MAX_ARDOP_BUFFER)
                sleep(1);

            if (tx_size > MAX_ARDOP_PACKET){
                ardop_size[0] = (uint8_t) (MAX_ARDOP_PACKET >> 8);
                ardop_size[1] = (uint8_t) (MAX_ARDOP_PACKET & 255);
            }
            else{
                ardop_size[0] = (uint8_t) (tx_size >> 8);
                ardop_size[1] = (uint8_t) (tx_size & 255);
            }

            // ardop header
            if (tcp_write(connector->data_socket, ardop_size, sizeof(ardop_size))
                == false)
            {
                fprintf(stderr, "Error in tcp_write(data_socket)\n");
                connector->shutdown = true;
                goto exit_local;
            }

           // fprintf(stderr, "ardop_data_worker_thread_tx: After ardop header tcp_write\n");
            bool ret;
            if (tx_size > MAX_ARDOP_PACKET)
            {
                fprintf(stderr,"Sent to ARDOP: %u bytes.\n", MAX_ARDOP_PACKET);
                ret = tcp_write(connector->data_socket, &buffer[counter] , MAX_ARDOP_PACKET);
                counter += MAX_ARDOP_PACKET;
                tx_size -= MAX_ARDOP_PACKET;
            }
            else{
                fprintf(stderr,"Sent to ARDOP: %u bytes.\n", tx_size);
                ret = tcp_write(connector->data_socket, &buffer[counter], tx_size);
                counter += tx_size;
                tx_size -= tx_size;
            }

            // check lost tcp connection
            if (ret == false)
            {
                fprintf(stderr, "Error in tcp_write(data_socket)\n");
                connector->shutdown = true;
                goto exit_local;
            }

            // buffer management hack
            sleep(2);
        }

    }

exit_local:

    fprintf(stderr, "ardop_data_worker_thread_tx exit.\n");
    return EXIT_SUCCESS;
}

void *ardop_data_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[MAX_ARDOP_PACKET_SAFE];
    uint32_t buf_size; // our header is 4 bytes long
    uint8_t ardop_size[2];

    while(connector->shutdown == false){

        while (connector->connected == false){
            if (connector->shutdown == true){
                goto exit_local;
            }
            sleep(1);
        }

        // fprintf(stderr,"Before tcp_read.\n");
        ardop_size[0] = 0; ardop_size[1] = 0;
        if (tcp_read(connector->data_socket, ardop_size, 2) == false)
        {
            fprintf(stderr, "Error in tcp_read(data_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }

        // ARDOP TNC data format: length 2 bytes | payload
        buf_size = 0;
        buf_size = ardop_size[0];
        buf_size <<= 8;
        buf_size |= ardop_size[1];

        fprintf(stderr,"Ardop Receive: %u bytes.\n", buf_size);

        if (tcp_read(connector->data_socket, buffer, buf_size) == false)
        {
            fprintf(stderr, "Error in tcp_read(data_socket)\n");
            connector->shutdown = true;
            goto exit_local;
        }

        if (buf_size > 3 && !memcmp("ARQ", buffer,  3)){
            buf_size -= 3;
            while (circular_buf_free_size(connector->out_buffer) < buf_size)
            {
                usleep(10000); // 10ms
            }
            circular_buf_put_range(connector->out_buffer, buffer + 3, buf_size);
            // fprintf(stderr,"Buffer write: %u received.\n", buf_size);
            // fwrite(buffer+3, buf_size, 1, stdout);
        }
        else{
            buffer[buf_size] = 0;
            fprintf(stderr, "Ardop non-payload data rx: %s\n", buffer);
        }

    }

exit_local:
    fprintf(stderr, "ardop_data_worker_thread_rx exit.\n");
    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread_rx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t rcv_byte;
    uint8_t buffer[1024];
    int counter = 0;
    bool new_cmd = false;

    while(connector->shutdown == false){

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

// treat "STATUS CONNECT TO PP2UIT FAILED!" ?
// and reset waiting for connection!

        if (new_cmd){
            if (!memcmp(buffer, "DISCONNECTED", strlen("DISCONNECTED"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->clean_buffers = true;
                connector->connected = false;
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "NEWSTATE DISC", strlen("NEWSTATE DISC"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->clean_buffers = true;
                connector->connected = false;
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "CONNECTED", strlen("CONNECTED"))){
                fprintf(stderr, "TNC: %s\n", buffer);
                connector->connected = true;
                if (connector->waiting_for_connection == false)
                { // we are receiving a connection... call uucico!
                    bool retval = call_uucico(connector);
                    if (retval == false)
                        fprintf(stderr, "Error calling call_uucico()!\n");
                }
                connector->waiting_for_connection = false;
            } else
            if (!memcmp(buffer, "PTT", strlen("PTT"))){
                // supressed output
                // fprintf(stderr, "%s -- CMD NOT CONSIDERED!!\n", buffer);
            } else
            if (!memcmp(buffer, "BUFFER", strlen("BUFFER"))){
                sscanf( (char *) buffer, "BUFFER %d", & connector->buffer_size);
                fprintf(stderr, "BUFFER: %d\n", connector->buffer_size);

            } else
                if (!memcmp(buffer, "INPUTPEAKS", strlen("INPUTPEAKS"))){
                    // suppressed output
                } else {
                    fprintf(stderr, "%s\n", buffer);
                }
        }
    }

exit_local:
    fprintf(stderr, "ardop_control_worker_thread_rx exit.\n");
    return EXIT_SUCCESS;
}

void *ardop_control_worker_thread_tx(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    char buffer[1024];
    bool ret = true;

    // initialize
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "INITIALIZE\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    // We set a call sign
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer, "MYCALL %s\r", connector->call_sign);
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    // we take care of timeout, here we just set the wanted timeout + 5
    memset(buffer,0,sizeof(buffer));
    if (connector->timeout < 240)
        sprintf(buffer, "ARQTIMEOUT %d\r", connector->timeout);
    else
        sprintf(buffer, "ARQTIMEOUT %d\r", MAX_TIMEOUT);
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

//    memset(buffer,0,sizeof(buffer));
//    sprintf(buffer, "ARQBW 2000MAX\r");
//    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer)); 

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"LISTEN True\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    strcpy(buffer,"BUSYDET 0\r"); // disabling busy detection
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    memset(buffer,0,sizeof(buffer));
    if (connector->ofdm_mode == true)
        strcpy(buffer,"ENABLEOFDM True\r");
    else
        strcpy(buffer,"ENABLEOFDM False\r");
    ret &= tcp_write(connector->control_socket, (uint8_t *) buffer, strlen(buffer));

    // check lost tcp connection
    if (ret == false)
    {
        fprintf(stderr, "Error in tcp_write(control_socket)\n");
        connector->shutdown = true;
        goto exit_local;
    }


    // 1Hz function
    uint32_t counter = 0;
    while(connector->shutdown == false){

        ret = true;
        // Logic to start a connection

        // lets issue buffer commands.... but not much!
        if (connector->connected == true && counter % 2) {
            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"BUFFER\r");
            ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));
        }

        if (connector->clean_buffers == true)
        {
            if (connector->connected == true)
            {
                if (connector->send_break == true) // we might have some data from uucico
                {
                    fprintf(stderr, "Sending BREAK to ardop.\n");
                    memset(buffer,0,sizeof(buffer));
                    sprintf(buffer,"BREAK\r");
                    ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));
                }

                connector->send_break = false;
                sleep(1);
                memset(buffer,0,sizeof(buffer));
                sprintf(buffer,"DISCONNECT\r");
                ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));
            }

            usleep(1200000); // sleep for threads finish their jobs (more than 1s here)

            fprintf(stderr, "Killing uucico.\n");
            system("killall uucico");

            fprintf(stderr, "Killing uuport.\n");
            system("killall uuport");

            fprintf(stderr, "Connection closed - Cleaning internal buffers.\n");
            circular_buf_reset(connector->in_buffer);
            circular_buf_reset(connector->out_buffer);

            while (connector->connected == true)
                usleep(100000);

            usleep(1200000); // sleep for threads finish their jobs (more than 1s here)

            fprintf(stderr, "Connection closed - Cleaning internal buffers 2.\n");
            circular_buf_reset(connector->in_buffer);
            circular_buf_reset(connector->out_buffer);

            fprintf(stderr, "Cleaning ardop buffer.\n");
            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"PURGEBUFFER\r");
            ret &= tcp_write(connector->control_socket, (uint8_t *)buffer, strlen(buffer));

            connector->clean_buffers = false;
        }

        if (connector->connected == false &&
            circular_buf_size(connector->in_buffer) > 0 &&
            !connector->waiting_for_connection){

            memset(buffer,0,sizeof(buffer));
            sprintf(buffer,"ARQCALL %s 4\r", connector->remote_call_sign);
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
        counter++;
    }

exit_local:
    fprintf(stderr, "ardop_control_worker_thread_tx exit.\n");
    return EXIT_SUCCESS;
}

bool initialize_modem_ardop(rhizo_conn *connector){

    if (tcp_connect(connector->ip_address, connector->tcp_base_port, &connector->control_socket)
        == false)
    {
        fprintf(stderr, "Connection to TNC's control socket failed.\n");
        connector->shutdown = true;
        return false;
    }

    if (tcp_connect(connector->ip_address, connector->tcp_base_port+1, &connector->data_socket)
        == false)
    {
        fprintf(stderr, "Connection to TNC's data socket failed.\n");
        connector->shutdown = true;
        return false;
    }

    sys_led_on(connector->serial_fd, connector->radio_type);

    // we start our control rx thread
    pthread_t tid1;
    pthread_create(&tid1, NULL, ardop_control_worker_thread_rx, (void *) connector);

    // we start our control tx thread
    pthread_t tid2;
    pthread_create(&tid2, NULL, ardop_control_worker_thread_tx, (void *) connector);

    // and run the two workers for the data channel
    pthread_t tid3;
    pthread_create(&tid3, NULL, ardop_data_worker_thread_tx, (void *) connector);

    pthread_t tid4;
    pthread_create(&tid4, NULL, ardop_data_worker_thread_rx, (void *) connector);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);

    // should we write a tcp_write (CLOSE) to the TNC?

    return true;
}
