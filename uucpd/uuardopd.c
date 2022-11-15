/* Rhizo-uuhf: Tools to integrate HF TNCs to UUCP
 * Copyright (C) 2019-2022 Rhizomatica
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
 * @file uuardopd.c
 * @author Rafael Diniz
 * @date 10 Jul 2019
 * @brief UUCP ARDOP daemon
 *
 * UUARDOPD main C file.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>

#include "serial.h"
#include "call_uucico.h"
#include "uuardopd.h"
#include "ardop.h"
#include "vara.h"
#include "shm.h"
#include "circular_buffer.h"

#include "../trx_v1-userland/ubitx_controller.h"

// temporary global variable to enable sockets closure
rhizo_conn *tmp_conn = NULL;

// radio shm connector made easy...
controller_conn *radio_conn = NULL;

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    /* Do house cleaning work here */
    if (tmp_conn){
        if (tmp_conn->data_socket){
            shutdown(tmp_conn->data_socket, SHUT_RDWR);
            close (tmp_conn->data_socket);
        }
        if (tmp_conn->control_socket){
            shutdown(tmp_conn->control_socket, SHUT_RDWR);
            close (tmp_conn->control_socket);
        }
    }

    // clean buffers...
    circular_buf_reset(tmp_conn->in_buffer);
    circular_buf_reset(tmp_conn->out_buffer);

    tmp_conn->shutdown = true;

    connected_led_off(tmp_conn->serial_fd, tmp_conn->radio_type);
    sys_led_off(tmp_conn->serial_fd, tmp_conn->radio_type);

    // TODO: close the pipes here
    // join all the threads?

    exit(EXIT_SUCCESS);
}

void *modem_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;

    if (!strcmp("vara", connector->modem_type)){
        initialize_modem_vara(connector);
    }

    if (!strcmp("ardop", connector->modem_type)){
        initialize_modem_ardop(connector);
    }

    return NULL;
}

bool initialize_connector(rhizo_conn *connector){

    connector->in_buffer = circular_buf_init_shm(INTERNAL_BUFFER_SIZE,  SYSV_SHM_KEY_IB);
    connector->out_buffer = circular_buf_init_shm(INTERNAL_BUFFER_SIZE, SYSV_SHM_KEY_OB);

    connector->connected = false;
    connector->waiting_for_connection = false;
    connector->serial_keying = false;
    connector->serial_fd = -1;
    connector->ask_login = false;
    connector->clean_buffers = false; // <-  this also means -> ask for disconnection!

    connector->shutdown = false;

    connector->radio_type = RADIO_TYPE_SHM;
    connector->timeout = TIMEOUT_DEFAULT;
    connector->ofdm_mode = true;
    connector->vara_mode = 2300;
    connector->buffer_size = 0;
    connector->session_counter_read = 0;
    connector->session_counter_write = 0;

    connector->call_sign[0] = 0;        // --> set default to uucp nodename
    connector->remote_call_sign[0] = 0; // --> set default to CQ

    connector->uucico_active = false;
    connector->send_break = false;

    return true;
}

bool get_call_sign_from_uucp(rhizo_conn *connector)
{
    FILE *uuconf = fopen(UUCP_CONFIG, "r");
    if (uuconf == NULL)
    {
        fprintf(stderr, "No Call sign specified and UUCP config could not be opened.\n");
        return false;
    }

    int j, i;
    char buff_call[BUFFER_SIZE];
    char local_buff[BUFFER_SIZE];
    while(fgets(buff_call, BUFFER_SIZE, uuconf))
    {
        i = 0; j = 0;
        while (buff_call[i] != '\n')
        {
            if(buff_call[i] == '#' || isblank(buff_call[i]))
            {
                j = 0;
                break;
            }

            if(isalnum(buff_call[i]))
            {
                local_buff[j] = buff_call[i];
                local_buff[++j] = 0;
            }
            i++;

            if (!strncmp("nodename", local_buff, 8))
            {
                while (buff_call[i] == ' ')
                    i++;
                sscanf(&buff_call[i], "%s", connector->call_sign);
                goto got_callsign;
            }
        }
    }

    if (connector->call_sign[0] == 0)
    {
        fprintf(stderr, "No Call sign specified and could not read call sign from UUCP config.\n");
        fclose(uuconf);
        return false;
    }

got_callsign:
    fclose(uuconf);
    return true;

}

int main (int argc, char *argv[])
{
    rhizo_conn *connector = NULL;

    if (shm_is_created(SYSV_SHM_KEY_STR, sizeof(rhizo_conn)))
    {
        fprintf(stderr, "Connector SHM is already created!\nDestroying it and creating again.\n");
        shm_destroy(SYSV_SHM_KEY_STR, sizeof(rhizo_conn));
    }
    shm_create(SYSV_SHM_KEY_STR, sizeof(rhizo_conn));

    connector = shm_attach(SYSV_SHM_KEY_STR, sizeof(rhizo_conn));
    tmp_conn = connector;

    initialize_connector(connector);

    signal (SIGINT, finish);
    signal (SIGQUIT, finish);
    signal (SIGTERM, finish);

    // Catch SIGPIPE
    // signal (SIGPIPE, pipe_fucked);
    signal(SIGPIPE, SIG_IGN); // ignores SIGPIPE...

    fprintf(stderr, "Rhizomatica's uuardopd version 0.2 by Rafael Diniz -  rafael (AT) rhizomatica (DOT) org\n");
    fprintf(stderr, "License: GNU AGPL version 3+\n\n");


    if (argc < 7)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s -r vara -a tnc_ip_address -p tcp_base_port -s /dev/ttyUSB0 [-l]\n", argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -r [ardop,vara]            Choose modem/radio type.\n");
        fprintf(stderr, " -c callsign                    Station Callsign (Eg: PU2HFF). Not setting it will cause the hostname to be retrieved from uucp config\n");
        fprintf(stderr, " -d remote_callsign      Remote Station Callsign.\n");
        fprintf(stderr, " -a tnc_ip_address        IP address of the TNC,\n");
        fprintf(stderr, " -p tnc_tcp_base_port  TNC's TCP base port of the TNC. ARDOP uses ports tcp_base_port and tcp_base_port+1.\n");
        fprintf(stderr, " -t timeout                    Time to wait before disconnect when idling (only for ardop).\n");
        fprintf(stderr, " -f features                   Supported features ARDOP: ofdm, noofdm (default: ofdm).\n");
        fprintf(stderr, "                                     Supported features VARA, BW mode: 500, 2300 or 2750 (default: 2300).\n");
        fprintf(stderr, " -s serial_device           Set the serial device file path for keying the radio (VARA ONLY).\n");
        fprintf(stderr, " -l                                  Tell UUCICO to ask login prompt (default: disabled).\n");
        fprintf(stderr, " -o [icom,ubitx,shm]     Sets radio type (supported: icom, ubitx or shm)\n");
        fprintf(stderr, " -h                                 Prints this help.\n");
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hlc:d:p:a:t:f:o:r:s:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            goto manual;
            break;
        case 'l':
            connector->ask_login = true;
            break;
        case 'r':
            strcpy(connector->modem_type, optarg);
            break;
        case 'o':
            if (!strcmp(optarg,"icom"))
                connector->radio_type = RADIO_TYPE_ICOM;
            if (!strcmp(optarg,"ubitx"))
                connector->radio_type = RADIO_TYPE_UBITX;
            if (!strcmp(optarg,"shm"))
                connector->radio_type = RADIO_TYPE_SHM;
            break;
        case 'c':
            strcpy(connector->call_sign, optarg);
            break;
        case 'd':
            strcpy(connector->remote_call_sign, optarg);
            break;
        case 't':
            connector->timeout = atoi(optarg);
            break;
        case 'p':
            connector->tcp_base_port = atoi(optarg);
            break;
        case 'a':
            strcpy(connector->ip_address, optarg);
            break;
        case 's':
            connector->serial_keying = true;
            strcpy(connector->serial_path, optarg);
            break;
        case 'f':
            if(strstr(optarg, "noofdm"))
                connector->ofdm_mode = false;
            else if(strstr(optarg, "ofdm"))
                connector->ofdm_mode = true;
            else
                connector->vara_mode = atoi(optarg);
            break;
        default:
            goto manual;
        }
    }

    if (connector->call_sign[0] == 0)
    {
        if (get_call_sign_from_uucp(connector) == false)
            goto manual;
        fprintf(stderr, "Call-sign obtained from uucp config: %s\n", connector->call_sign);
    }

    if (connector->remote_call_sign[0] == 0)
    {
        strcpy(connector->remote_call_sign, "CQ");
//        fprintf(stderr, "Destination call-sign not set. Using CQ.\n");
    }

    if (!strcmp("vara", connector->modem_type) &&
        (connector->vara_mode != 500)  &&
        (connector->vara_mode != 2300) &&
        (connector->vara_mode != 2750) )
    {
        fprintf(stderr, "Wrong Vara Mode BW %u.\n", connector->vara_mode);
        goto manual;
    }

    controller_conn *shm_radio_connector = NULL;
    if (connector->radio_type == RADIO_TYPE_SHM)
    {
        if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)) == false)
        {
            fprintf(stderr, "SHM Radio Connector SHM not created. Is ubitx_controller running?\n");
            return EXIT_FAILURE;
        }
        shm_radio_connector = shm_attach(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
        radio_conn = shm_radio_connector;
    }

    connected_led_off(connector->serial_fd, connector->radio_type);

    pthread_t tid;
    pthread_create(&tid, NULL, uucico_thread, (void *) connector);

    modem_thread((void *) connector);
    fprintf(stderr, "Modem connection lost.\n");

    connector->shutdown = true;
    // workaround... this was not supposed to be the last line of code...
    finish(0);
    return EXIT_SUCCESS;

    // should we try to reconnect!
#if 0
    if ((connector->shutdown == true) && reconnect)
    {
        pthread_cancel(everybody);
        ring_buffer_clean(all_buffers);
        goto start_again;
    }
#endif

    pthread_join(tid, NULL);
    fprintf(stderr, "uucico recv listener thread finish.\n");

    return EXIT_SUCCESS;
}
