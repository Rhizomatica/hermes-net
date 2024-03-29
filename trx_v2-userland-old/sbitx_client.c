/* sbitx_client
 * Copyright (C) 2023-2024 Rhizomatica
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
#include <unistd.h>

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <threads.h>
#include <pthread.h>

#include "sbitx_controller.h"
#include "sbitx_io.h"
#include "../include/radio_cmds.h"

#include "shm.h"
#include "help.h"

controller_conn *tmp_connector = NULL;

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    if (tmp_connector)
    {
        pthread_mutex_unlock(&tmp_connector->cmd_mutex);
        pthread_mutex_unlock(&tmp_connector->response_mutex);
    }

    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{
    controller_conn *connector = NULL;
    char command[64];
    char command_argument[64];
    uint8_t srv_cmd[5];
    uint8_t response[5];
    bool argument_set = false;

    if (argc < 3)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s -c command [-a command_argument]\n", argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -c command                 Runs the specified command\n");
        fprintf(stderr, " -a command_argument        Runs the specified command\n");
        fprintf(stderr, " -h                         Prints this help.\n");
        fprintf(stderr, "\nList of commands, arguments and responses (respectivelly):\n\n");
        fprintf(stderr, format_str);
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hc:a:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            goto manual;
            break;
        case 'c':
            strcpy(command, optarg);
            break;
        case 'a':
            strcpy(command_argument, optarg);
            argument_set = true;
            break;
        default:
            goto manual;
        }
    }

    memset(srv_cmd, 0, 5);

    if (!strcmp(command, "ptt_on"))
    {
        srv_cmd[4] = CMD_PTT_ON;
    }
    else if (!strcmp(command, "ptt_off"))
    {
        srv_cmd[4] = CMD_PTT_OFF;
    }
    else if (!strcmp(command, "get_frequency"))
    {
        srv_cmd[4] = CMD_GET_FREQ;
    }
    else if (!strcmp(command, "set_frequency"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t freq = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &freq, 4);
        srv_cmd[4] = CMD_SET_FREQ;
    }
    else if (!strcmp(command, "get_mode"))
    {
        srv_cmd[4] = CMD_GET_MODE;
    }
    else if (!strcmp(command, "set_mode"))
    {
        if (argument_set == false)
            goto manual;

        if (!strcmp(command_argument, "lsb") || !strcmp(command_argument, "LSB"))
            srv_cmd[0] = 0x00;

        if (!strcmp(command_argument, "usb") || !strcmp(command_argument, "USB"))
            srv_cmd[0] = 0x01;

        if (!strcmp(command_argument, "cw") || !strcmp(command_argument, "CW"))
            srv_cmd[0] = 0x04;

        srv_cmd[4] = CMD_SET_MODE;
    }
    else if (!strcmp(command, "get_txrx_status"))
    {
        srv_cmd[4] = CMD_GET_TXRX_STATUS;
    }
    else if (!strcmp(command, "get_protection_status"))
    {
        srv_cmd[4] = CMD_GET_PROTECTION_STATUS;
    }
    else if (!strcmp(command, "get_mastercal"))
    {
        srv_cmd[4] = CMD_GET_MASTERCAL;
    }
    else if (!strcmp(command, "set_mastercal"))
    {
        if (argument_set == false)
            goto manual;

        int32_t freq = atoi(command_argument);
        memcpy(srv_cmd, &freq, 4);
        srv_cmd[4] = CMD_SET_MASTERCAL;
    }
    else if (!strcmp(command, "get_bfo"))
    {
        srv_cmd[4] = CMD_GET_BFO;
    }
    else if (!strcmp(command, "set_bfo"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t freq = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &freq, 4);
        srv_cmd[4] = CMD_SET_BFO;
    }
    else if (!strcmp(command, "get_fwd"))
    {
        srv_cmd[4] = CMD_GET_FWD;
    }
    else if (!strcmp(command, "get_ref"))
    {
        srv_cmd[4] = CMD_GET_REF;
    }
    else if (!strcmp(command, "get_led_status"))
    {
        srv_cmd[4] = CMD_GET_LED_STATUS;
    }
    else if (!strcmp(command, "set_led_status"))
    {
        if (argument_set == false)
            goto manual;

        if (!strcmp(command_argument, "1") || !strcmp(command_argument, "true") || !strcmp(command_argument, "on") || !strcmp(command_argument, "ON"))
            srv_cmd[0] = 0x01;

        if (!strcmp(command_argument, "0") || !strcmp(command_argument, "false") || !strcmp(command_argument, "off") || !strcmp(command_argument, "OFF"))
            srv_cmd[0] = 0x00;

        srv_cmd[4] = CMD_SET_LED_STATUS;
    }
    else if (!strcmp(command, "get_connected_status"))
    {
        srv_cmd[4] = CMD_GET_CONNECTED_STATUS;
    }
    else if (!strcmp(command, "set_connected_status"))
    {
        if (argument_set == false)
            goto manual;

        if (!strcmp(command_argument, "1") || !strcmp(command_argument, "true") || !strcmp(command_argument, "on") || !strcmp(command_argument, "ON"))
            srv_cmd[0] = 0x01;

        if (!strcmp(command_argument, "0") || !strcmp(command_argument, "false") || !strcmp(command_argument, "off") || !strcmp(command_argument, "OFF"))
            srv_cmd[0] = 0x00;

        srv_cmd[4] = CMD_SET_CONNECTED_STATUS;
    }
    else if (!strcmp(command, "get_serial"))
    {
        srv_cmd[4] = CMD_GET_SERIAL;
    }
    else if (!strcmp(command, "set_serial"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t serial = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &serial, 4);
        srv_cmd[4] = CMD_SET_SERIAL;
    }
    else if (!strcmp(command, "reset_protection"))
    {
        srv_cmd[4] = CMD_RESET_PROTECTION;
    }
    else if (!strcmp(command, "set_ref_threshold"))
    {
        if (argument_set == false)
            goto manual;

        uint16_t ref_threshold = (uint16_t) atoi(command_argument);
        memcpy(srv_cmd, &ref_threshold, 2);
        srv_cmd[4] = CMD_SET_REF_THRESHOLD;
    }
    else if (!strcmp(command, "get_ref_threshold"))
    {
        srv_cmd[4] = CMD_GET_REF_THRESHOLD;
    }
    else if (!strcmp(command, "get_freqstep"))
    {
        srv_cmd[4] = CMD_GET_STEPHZ;
    }
    else if (!strcmp(command, "set_freqstep"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t stephz = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &stephz, 4);
        srv_cmd[4] = CMD_SET_STEPHZ;
    }
    else if (!strcmp(command, "get_volume"))
    {
        srv_cmd[4] = CMD_GET_VOLUME;
    }
    else if (!strcmp(command, "set_volume"))
    {
        if (argument_set == false)
            goto manual;

        int volume = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &volume, 4);
        srv_cmd[4] = CMD_SET_VOLUME;
    }
    else if (!strcmp(command, "get_tone"))
    {
        srv_cmd[4] = CMD_GET_TONE;
    }
    else if (!strcmp(command, "set_tone"))
    {
        if (argument_set == false)
            goto manual;

        uint8_t tone = (uint8_t) atoi(command_argument);
        memcpy(srv_cmd, &tone, 1);
        srv_cmd[4] = CMD_SET_TONE;
    }
    else if (!strcmp(command, "set_radio_defaults"))
    {
        srv_cmd[4] = CMD_SET_RADIO_DEFAULTS;
    }
    else if (!strcmp(command, "gps_calibrate"))
    {
        srv_cmd[4] = CMD_GPS_CALIBRATE;
    }
    else if (!strcmp(command, "restore_radio_defaults"))
    {
        srv_cmd[4] = CMD_RESTORE_RADIO_DEFAULTS;
    }
    else if (!strcmp(command, "get_status"))
    {
        srv_cmd[4] = CMD_GET_STATUS;
    }
    else if (!strcmp(command, "radio_reset"))
    {
        srv_cmd[4] = CMD_RADIO_RESET;
    }
    else
    {
        printf("WRONG_COMMAND\n");
        goto manual;
    }

    signal (SIGINT, finish);
    signal (SIGQUIT, finish);
    signal (SIGTERM, finish);

    if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)) == false)
    {
        fprintf(stderr, "Connector SHM not created. Is sbitx_controller running?\n");
        return EXIT_FAILURE;
    }

    connector = shm_attach(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
    tmp_connector = connector;

    bool cmd_resp = radio_cmd(connector, srv_cmd, response);

    if (srv_cmd[4] == CMD_RADIO_RESET)
    {
        printf("OK\n");
        return EXIT_SUCCESS;
    }

    if (cmd_resp == false)
        printf("ERROR\n");
    else
    {
        uint32_t status;
        uint32_t freq;
        uint32_t freqstep;
        uint32_t serial;
        uint16_t measure;
        uint8_t tone;


        switch(response[0])
        {
        case CMD_RESP_ACK:
            printf("OK\n");
            break;
        case CMD_RESP_PTT_ON_NACK:
        case CMD_RESP_PTT_OFF_NACK:
            printf("NOK\n");
            break;
        case CMD_ALERT_PROTECTION_ON:
            printf("SWR\n");
            break;
        case CMD_RESP_GET_MODE_USB:
            printf("USB\n");
            break;
        case CMD_RESP_GET_MODE_LSB:
            printf("LSB\n");
            break;
        case CMD_RESP_GET_MODE_CW:
            printf("CW\n");
            break;
        case CMD_RESP_GET_TXRX_INTX:
            printf("INTX\n");
            break;
        case CMD_RESP_GET_TXRX_INRX:
            printf("INRX\n");
            break;
        case CMD_RESP_GET_LED_STATUS_OFF:
            printf("LED_OFF\n");
            break;
        case CMD_RESP_GET_LED_STATUS_ON:
            printf("LED_ON\n");
            break;
        case CMD_RESP_GET_PROTECTION_ON:
            printf("PROTECTION_ON\n");
            break;
        case CMD_RESP_GET_PROTECTION_OFF:
            printf("PROTECTION_OFF\n");
            break;
        case CMD_RESP_GET_CONNECTED_STATUS_ON:
            printf("LED_ON\n");
            break;
        case CMD_RESP_GET_CONNECTED_STATUS_OFF:
            printf("LED_OFF\n");
            break;
        case CMD_RESP_GPS_NOT_PRESENT:
            printf("NO_GPS\n");
            break;
        case CMD_RESP_GET_FREQ_ACK:
            memcpy (&freq, connector->response_service+1, 4);
            printf("%u\n", freq);
            break;
        case CMD_RESP_GET_MASTERCAL_ACK:
            memcpy (&freq, connector->response_service+1, 4);
            printf("%d\n", freq);
            break;
        case CMD_RESP_GET_SERIAL_ACK:
            memcpy (&serial, connector->response_service+1, 4);
            printf("%u\n", serial);
            break;
        case CMD_RESP_GET_STEPHZ_ACK:
            memcpy (&freqstep, connector->response_service+1, 4);
            printf("%u\n", freqstep);
            break;
        case CMD_RESP_GET_VOLUME_ACK:
            memcpy (&status, connector->response_service+1, 4);
            printf("%u\n", status);
            break;
        case CMD_RESP_GET_TONE_ACK:
            memcpy (&tone, connector->response_service+1, 1);
            printf("%hhu\n", tone);
            break;
        case CMD_RESP_GET_BFO_ACK:
            memcpy (&freq, connector->response_service+1, 4);
            printf("%u\n", freq);
            break;
        case CMD_RESP_GET_FWD_ACK:
            memcpy (&measure, connector->response_service+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_REF_ACK:
            memcpy (&measure, connector->response_service+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_REF_THRESHOLD_ACK:
            memcpy (&measure, connector->response_service+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_STATUS_ACK:
            memcpy (&status, connector->response_service+1, 4);
            bool pps_status = (status >> 20) & 0x1;
            bool offset_adjustment_status = (status >> 21) & 0x1;
            int32_t offset = status & 0x7ffffL;
            bool offset_neg_sign = (status >> 19) & 0x1;
            if (offset_neg_sign)
                offset = -offset;
            printf("PPS_STATUS %s\n", pps_status ? "OK":"FAIL");
            printf("OFFSET_CAL_STATUS %s\n", offset_adjustment_status ? "OK":"FAIL");
            printf("LATEST_OFFSET_CAL %d\n", offset);
            break;

        case CMD_RESP_WRONG_COMMAND:
        default:
            printf("ERROR\n");
        }

    }

    return EXIT_SUCCESS;
}
