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

#include "sbitx_shm.h"
#include "sbitx_io.h"
#include "shm_utils.h"

#include "help.h"

#include "radio_cmds.h"

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
    uint8_t profile = 0; // this is the default profile, if not set
    uint8_t srv_cmd[5];
    uint8_t response[5];
    bool argument_set = false;
    bool show_connector_msg = false;

    if (argc < 3)
    {
    manual:
        printf("Usage modes: \n%s -c command [-a command_argument] [-p profile_number]\n", argv[0]);
        printf("%s -h\n", argv[0]);
        printf("\nOptions:\n");
        printf(" -c command                 Runs the specified command\n");
        printf(" -a command_argument        Argument of the command\n");
        printf(" -p profile_number          Select the profile to apply the command (only selected commands)\n");
        printf(" -h                         Prints this help.\n");
        printf("\nList of commands, followed by the need for profile indication, arguments and responses (respectivelly):\n\n");
        printf(format_str);
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hc:a:p:")) != -1)
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
        case 'p':
            profile = (uint8_t) atoi(optarg);
            break;

        default:
            goto manual;
        }
    }

    memset(srv_cmd, 0, 5);


    if (!strcmp(command, "get_message"))
    {
        show_connector_msg = true;
    }
    else if (!strcmp(command, "ptt_on"))
    {
        srv_cmd[4] = CMD_PTT_ON;
    }
    else if (!strcmp(command, "ptt_off"))
    {
        srv_cmd[4] = CMD_PTT_OFF;
    }
    else if(!strcmp(command, "get_profile"))
    {
        srv_cmd[4] = CMD_GET_PROFILE;
    }
    else if(!strcmp(command, "set_profile"))
    {
        if (argument_set == false)
            goto manual;

        srv_cmd[0] = (uint8_t) atoi(command_argument);
        srv_cmd[4] = CMD_SET_PROFILE;
    }
    else if (!strcmp(command, "get_frequency"))
    {
        srv_cmd[4] = CMD_GET_FREQ | (profile << 6);
    }
    else if (!strcmp(command, "set_frequency"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t freq = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &freq, 4);
        srv_cmd[4] = CMD_SET_FREQ | (profile << 6);
    }
    else if (!strcmp(command, "get_mode"))
    {
        srv_cmd[4] = CMD_GET_MODE | (profile << 6);
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

        srv_cmd[4] = CMD_SET_MODE | (profile << 6);
    }
    else if (!strcmp(command, "get_snr"))
    {
        srv_cmd[4] = CMD_GET_SNR;
    }
    else if (!strcmp(command, "set_snr"))
    {
        if (argument_set == false)
            goto manual;

        int32_t snr = (int32_t) atoi(command_argument);
        memcpy(srv_cmd, &snr, 4);

        srv_cmd[4] = CMD_SET_SNR;
    }
    else if (!strcmp(command, "get_bitrate"))
    {
        srv_cmd[4] = CMD_GET_BITRATE;
    }
    else if (!strcmp(command, "set_bitrate"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t bitrate = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &bitrate, 4);

        srv_cmd[4] = CMD_SET_BITRATE;
    }
    else if (!strcmp(command, "get_bytes_rx"))
    {
        srv_cmd[4] = CMD_GET_BYTES_RX;
    }
    else if (!strcmp(command, "set_bytes_rx"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t bytes = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &bytes, 4);

        srv_cmd[4] = CMD_SET_BYTES_RX;
    }
    else if (!strcmp(command, "get_bytes_tx"))
    {
        srv_cmd[4] = CMD_GET_BYTES_TX;
    }
    else if (!strcmp(command, "set_bytes_tx"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t bytes = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &bytes, 4);

        srv_cmd[4] = CMD_SET_BYTES_TX;
    }
	else if (!strcmp(command, "get_txrx_status"))
    {
        srv_cmd[4] = CMD_GET_TXRX_STATUS;
    }
    else if (!strcmp(command, "get_protection_status"))
    {
        srv_cmd[4] = CMD_GET_PROTECTION_STATUS;
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
    else if (!strcmp(command, "get_digital_voice"))
    {
        srv_cmd[4] = CMD_GET_DIGITAL_VOICE | (profile << 6);
    }
    else if (!strcmp(command, "set_digital_voice"))
    {
        if (argument_set == false)
            goto manual;

        if (!strcmp(command_argument, "1") || !strcmp(command_argument, "true") || !strcmp(command_argument, "on") || !strcmp(command_argument, "ON"))
            srv_cmd[0] = 0x01;

        if (!strcmp(command_argument, "0") || !strcmp(command_argument, "false") || !strcmp(command_argument, "off") || !strcmp(command_argument, "OFF"))
            srv_cmd[0] = 0x00;

        srv_cmd[4] = CMD_SET_DIGITAL_VOICE | (profile << 6);
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
    else if (!strcmp(command, "reset_timeout"))
    {
        srv_cmd[4] = CMD_TIMEOUT_RESET;
    }
    else if(!strcmp(command, "set_timeout"))
    {
        if (argument_set == false)
            goto manual;

        int32_t timeout = (int32_t) atoi(command_argument);
        memcpy(srv_cmd, &timeout, 4);
        srv_cmd[4] = CMD_SET_TIMEOUT;
    }
    else if(!strcmp(command, "get_timeout"))
    {
        srv_cmd[4] = CMD_GET_TIMEOUT;
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
        srv_cmd[4] = CMD_GET_VOLUME | (profile << 6);
    }
    else if (!strcmp(command, "set_volume"))
    {
        if (argument_set == false)
            goto manual;

        int volume = (uint32_t) atoi(command_argument);
        memcpy(srv_cmd, &volume, 4);
        srv_cmd[4] = CMD_SET_VOLUME | (profile << 6);
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
    else if (!strcmp(command, "get_power"))
    {
        srv_cmd[4] = CMD_GET_POWER | (profile << 6);
    }
    else if (!strcmp(command, "set_power"))
    {
        if (argument_set == false)
            goto manual;

        uint32_t power = (uint32_t) atoi(command_argument);
        if (power > 100 || power < 0)
        {
            fprintf(stderr, "Power must be between 0 and 100.\n");
            goto manual;
        }

        memcpy(srv_cmd, &power, 4);
        srv_cmd[4] = CMD_SET_POWER | (profile << 6);
    }
    else if (!strcmp(command, "set_radio_defaults"))
    {
        srv_cmd[4] = CMD_SET_RADIO_DEFAULTS;
    }
    else if (!strcmp(command, "restore_radio_defaults"))
    {
        srv_cmd[4] = CMD_RESTORE_RADIO_DEFAULTS;
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

    if (show_connector_msg)
    {
        printf("%s\n", connector->message);
        printf("message_available: %s\n", connector->message_available? "yes":"no");
        return EXIT_SUCCESS;
    }

    memset(response, 0, 5);
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
        uint32_t status, freq, freqstep, serial, power;
        int32_t timeout, snr;
        uint16_t measure;
        uint8_t tone, profile;


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
        case CMD_RESP_GET_DIGITAL_VOICE_ON:
            printf("ON\n");
            break;
        case CMD_RESP_GET_DIGITAL_VOICE_OFF:
            printf("OFF\n");
            break;
         case CMD_RESP_GET_FREQ_ACK:
            memcpy (&freq, response+1, 4);
            printf("%u\n", freq);
            break;
         case CMD_RESP_GET_SERIAL_ACK:
            memcpy (&serial, response+1, 4);
            printf("%u\n", serial);
            break;
        case CMD_RESP_GET_STEPHZ_ACK:
            memcpy (&freqstep, response+1, 4);
            printf("%u\n", freqstep);
            break;
        case CMD_RESP_GET_VOLUME_ACK:
            memcpy (&status, response+1, 4);
            printf("%u\n", status);
            break;
        case CMD_RESP_GET_TONE_ACK:
            memcpy (&tone, response+1, 1);
            printf("%hhu\n", tone);
            break;
        case CMD_RESP_GET_BFO_ACK:
            memcpy (&freq, response+1, 4);
            printf("%u\n", freq);
            break;
        case CMD_RESP_GET_FWD_ACK:
            memcpy (&measure, response+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_REF_ACK:
            memcpy (&measure, response+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_REF_THRESHOLD_ACK:
            memcpy (&measure, response+1, 2);
            printf("%hu\n", measure);
            break;
        case CMD_RESP_GET_PROFILE:
            memcpy (&profile, response+1, 1);
            printf("%hhu\n", profile);
            break;
        case CMD_RESP_GET_BITRATE:
            memcpy (&status, response+1, 4);
            printf("%u\n", status);
            break;
        case CMD_RESP_GET_SNR:
            memcpy (&snr, response+1, 4);
            printf("%d\n", snr);
            break;
        case CMD_RESP_GET_BYTES_RX:
            memcpy (&status, response+1, 4);
            printf("%u\n", status);
            break;
        case CMD_RESP_GET_BYTES_TX:
            memcpy (&status, response+1, 4);
            printf("%u\n", status);
            break;
        case CMD_RESP_GET_TIMEOUT_ACK:
            memcpy(&timeout, response+1, 4);
            printf("%d\n", timeout);
            break;
        case CMD_RESP_GET_POWER:
            memcpy(&power, response+1, 4);
            printf("%u\n", power);
            break;

            // this happens when there is no anwser from daemon
        case CMD_RESP_TIMEOUT:
            printf("TIMEOUT\n");
            break;

        case CMD_RESP_WRONG_COMMAND:
        default:
            printf("ERROR\n");
        }

    }

    return EXIT_SUCCESS;
}
