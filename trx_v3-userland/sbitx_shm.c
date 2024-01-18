/* sbitx_controller
 * Copyright (C) 2024 Rhizomatica
 * Author: Rafael Diniz <rafael@rhizomatica.org>
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

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <threads.h>
#include <pthread.h>

#include "sbitx_core.h"
#include "shm_utils.h"
#include "sbitx_shm.h"

#include "../include/radio_cmds.h"

extern _Atomic bool shutdown_;

static controller_conn *connector_local;

void process_radio_command(uint8_t *cmd, uint8_t *response)
{
    uint32_t frequency;
    char command[64];

    memset(response, 0, 5);

    // TODO: write me!!
    switch(cmd[4]){
#if 0
    case CMD_PTT_ON: // PTT On
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else if (in_tx == 0)
        {
            sound_input(1);
            tx_on(TX_SOFT);
            response[0] = CMD_RESP_PTT_ON_ACK;
        }
        else
        {
            response[0] = CMD_RESP_PTT_ON_NACK;
        }
        break;

    case CMD_PTT_OFF: // PTT OFF
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else
        if (in_tx != 0)
        {
            sound_input(0);
            tx_off();
            response[0] = CMD_RESP_PTT_OFF_ACK;
        }
        else
        {
            response[0] = CMD_RESP_PTT_OFF_NACK;
        }
        break;
    case CMD_GET_FREQ: // GET FREQUENCY
        response[0] = CMD_RESP_GET_FREQ_ACK;
        frequency = (uint32_t) get_freq();
        memcpy(response+1, &frequency, 4);
        break;

    case CMD_SET_FREQ: // SET FREQUENCY
        memcpy(&frequency, cmd, 4);

        sprintf(command, "r1:freq=%u", frequency);
        do_cmd(command);

        sprintf(command, "%u", frequency);
        set_field("r1:freq", command);
        set_field("#vfo_a_freq", command);

        response[0] = CMD_RESP_SET_FREQ_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_TXRX_STATUS: // GET TX/RX STATUS
        if (in_tx)
            response[0] = CMD_RESP_GET_TXRX_INTX;
        else
            response[0] = CMD_RESP_GET_TXRX_INRX;
        break;

    case CMD_SET_MODE: // set mode
        if (cmd[0] == 0x00 || cmd[0] == 0x03)
        {
            do_cmd("r1:mode=LSB");
            set_field("r1:mode", "LSB");
        }
        if (cmd[0] == 0x01)
        {
            do_cmd("r1:mode=USB");
            set_field("r1:mode", "USB");
        }
        if (cmd[0] == 0x04)
        {
            do_cmd("r1:mode=CW");
            set_field("r1:mode", "CW");
        }

        response[0] = CMD_RESP_SET_MODE_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_MODE: // GET SSB MODE
        if (rx_list->mode == MODE_USB)
            response[0] = CMD_RESP_GET_MODE_USB;
        if (rx_list->mode == MODE_LSB)
            response[0] = CMD_RESP_GET_MODE_LSB;
        if (rx_list->mode == MODE_CW)
            response[0] = CMD_RESP_GET_MODE_CW;
        break;

    case CMD_RESET_PROTECTION: // RESET PROTECTION
        response[0] = CMD_RESP_RESET_PROTECTION_ACK;
        is_swr_protect_enabled = false;
        send_ws_update = true;
        break;

    case CMD_GET_PROTECTION_STATUS: // GET PROTECTION STATUS
        if (is_swr_protect_enabled)
            response[0] = CMD_RESP_GET_PROTECTION_ON;
        else
            response[0] = CMD_RESP_GET_PROTECTION_OFF;
        break;

    case CMD_GET_MASTERCAL: // GET MASTER CAL
        response[0] = CMD_RESP_GET_MASTERCAL_ACK;
        memcpy(response+1, &frequency_offset, 4);
        break;

    case CMD_SET_MASTERCAL: // SET MASTER CAL
        memcpy(&frequency_offset, cmd, 4);
        response[0] = CMD_RESP_SET_MASTERCAL_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_BFO: // GET BFO
        response[0] = CMD_RESP_GET_BFO_ACK;
        memcpy(response+1, &bfo_freq, 4);
        break;

    case CMD_SET_BFO: // SET BFO
        memcpy(&bfo_freq, cmd, 4);
        response[0] = CMD_RESP_SET_BFO_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_FWD: // GET FWD
        response[0] = CMD_RESP_GET_FWD_ACK;
        memcpy(response+1, &fwdpower, 2);
        break;

    case CMD_GET_REF: // GET REF
        response[0] = CMD_RESP_GET_REF_ACK;
        memcpy(response+1, &vswr, 2);
        break;

    case CMD_GET_LED_STATUS: // GET LED STATUS
        if (led_status)
            response[0] = CMD_RESP_GET_LED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_LED_STATUS_OFF;
        break;

    case CMD_SET_LED_STATUS: // SET LED STATUS
        led_status = cmd[0];
        response[0] = CMD_RESP_SET_LED_STATUS_ACK;
        break;

    case CMD_GET_CONNECTED_STATUS: // GET CONNECTED STATUS
        if (connected_status)
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_OFF;
      break;

    case CMD_SET_CONNECTED_STATUS: // SET CONNECTED STATUS
        connected_status = cmd[0];
        response[0] = CMD_RESP_SET_CONNECTED_STATUS_ACK;
        break;

    case CMD_GET_SERIAL: // GET SERIAL NUMBER
        response[0] = CMD_RESP_GET_SERIAL_ACK;
        memcpy(response+1, &serial_number, 4);
      break;

    case CMD_SET_SERIAL: // SET SERIAL NUMBER
        memcpy(&serial_number, cmd, 4);
        response[0] = CMD_RESP_SET_SERIAL_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_STEPHZ: // CMD_GET_STEPHZ
        response[0] = CMD_RESP_GET_SERIAL_ACK;
        memcpy(response+1, &tuning_step, 4);
      break;

    case CMD_SET_STEPHZ: // CMD_SET_STEPHZ
        memcpy(&tuning_step, cmd, 4);
        response[0] = CMD_RESP_SET_STEPHZ_ACK;
        save_user_settings(1);
        break;

    case CMD_SET_REF_THRESHOLD: // CMD_SET_REF_THRESHOLD
        memcpy(&reflected_threshold, cmd, 2);
        response[0] = CMD_RESP_SET_REF_THRESHOLD_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_REF_THRESHOLD: // CMD_GET_REF_THRESHOLD
        response[0] = CMD_RESP_GET_REF_THRESHOLD_ACK;
        memcpy(response+1, &reflected_threshold, 2);
        break;

    case CMD_GET_VOLUME: // GET_VOLUME
        response[0] = CMD_RESP_GET_VOLUME_ACK;
        memcpy(response+1, &rx_vol, 4);
        break;

    case CMD_SET_VOLUME: // SET_VOLUME
        memcpy(&rx_vol, cmd, 4);
        if (rx_vol < 0) rx_vol = 0; if (rx_vol > 100) rx_vol = 100;
        sprintf(command, "%d", rx_vol);
        set_field("r1:volume", command);
        response[0] = CMD_RESP_SET_VOLUME_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_TONE: // GET_TONE
        response[0] = CMD_RESP_GET_TONE_ACK;
        memcpy(response+1, &tone_generation, 1);
        break;

    case CMD_SET_TONE: // SET_TONE
        memcpy(&tone_generation, cmd, 1);
        response[0] = CMD_RESP_SET_TONE_ACK;
        break;

#if 0
    case CMD_GET_OPERATING_MODE: // GET_OPERATING_MODE
        response[0] = CMD_RESP_GET_OPERATING_MODE_ACK;
        memcpy(response+1, &operating_mode, 2);
        break;

    case CMD_SET_OPERATING_MODE: // SET_OPERATING_MODE
        memcpy(&operating_mode, cmd, 2);
        response[0] = CMD_RESP_SET_OPERATING_MODE_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_FREQ_PHONE: // GET FREQUENCY
        response[0] = CMD_RESP_GET_FREQ_PHONE_ACK;
        memcpy(response+1, &freq_hdr_phone, 4);
        break;

    case CMD_SET_FREQ_PHONE: // SET FREQUENCY
//        memcpy(&frequency, cmd, 4);

//        sprintf(command, "r1:freq=%u", frequency);
//        do_cmd(command);

//        sprintf(command, "%u", frequency);
//        set_field("r1:freq", command);

        response[0] = CMD_RESP_SET_FREQ_PHONE_ACK;
        save_user_settings(1);
        break;


#endif

        // tx_drive (a multiplier of the samples)
        // tx_gain: "Capture" alsa for tx
        // rx_vol:  "Master" alsa
        // rx_gain: Capture alsa for rx

        // volume stuff, 0 to 100 (alsa level)
// GET_VOLUME // r1:volume // rx_vol
// SET_VOLUME // r1:volume //

        // 2 operating modes: Analog (phone) / Digital (data)
// GET_OPERATING_MODE
// SET_OPERATION_MODE

    case CMD_GPS_CALIBRATE: // CMD_GPS_CALIBRATE
        response[0] = CMD_RESP_GPS_NOT_PRESENT;
        break;

    case CMD_GET_STATUS: // CMD_GET_STATUS
        response[0] = CMD_RESP_GET_STATUS_ACK;
        memcpy(response+1, &radio_operation_result, 4);
        break;

    case CMD_SET_RADIO_DEFAULTS: // SET RADIO DEFAULTS
        save_user_settings(1);
        save_hw_settings();
        response[0] = CMD_RESP_SET_RADIO_DEFAULTS_ACK;
        break;

    case CMD_RESTORE_RADIO_DEFAULTS: // RESTORE RADIO DEFAULTS
        load_user_settings();
        read_hw_ini();
        response[0] = CMD_RESP_RESTORE_RADIO_DEFAULTS_ACK;
        break;
    case CMD_RADIO_RESET: // RADIO RESET
        exit(-1);
        break;
#endif
    default:
        response[0] = CMD_RESP_WRONG_COMMAND;
    }

}

void *process_radio_command_thread(void *arg)
{
    controller_conn *conn = arg;

    pthread_mutex_lock(&conn->cmd_mutex);

    while(!shutdown_)
    {

        pthread_cond_wait(&conn->cmd_condition, &conn->cmd_mutex);

        if(shutdown_)
            goto exit_local;

        process_radio_command(conn->service_command, conn->response_service);

        if (conn->service_command[4] == CMD_RADIO_RESET)
        {
            shutdown_ = true;
            pthread_mutex_unlock(&conn->cmd_mutex);
            fprintf(stderr,"\nReset command. Exiting\n");
        }

        conn->response_available = true;

    }

exit_local:
    pthread_mutex_unlock(&conn->cmd_mutex);

    return NULL;
}

bool initialize_connector(controller_conn *connector)
{
    // init mutexes
    pthread_mutex_t *mutex_ptr = (pthread_mutex_t *) & connector->cmd_mutex;

    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr))
    {
        perror("pthread_mutexattr_init");
        return false;
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED))
    {
        perror("pthread_mutexattr_setpshared");
        return false;
    }

    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST))
    {
        perror("pthread_mutexattr_setrobust");
        return false;
    }

    if (pthread_mutex_init(mutex_ptr, &attr))
    {
        perror("pthread_mutex_init");
        return false;
    }

    mutex_ptr = (pthread_mutex_t *) & connector->response_mutex;

    if (pthread_mutex_init(mutex_ptr, &attr))
    {
        perror("pthread_mutex_init");
        return false;
    }

    if (pthread_mutexattr_destroy(&attr))
    {
        perror("pthread_mutexattr_destroy");
        return false;
    }

    // init the cond
    pthread_cond_t *cond_ptr = (pthread_cond_t *) & connector->cmd_condition;
    pthread_condattr_t cond_attr;
    if (pthread_condattr_init(&cond_attr))
    {
        perror("pthread_condattr_init");
        return false;
    }
    if (pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED))
    {
        perror("pthread_condattr_setpshared");
        return false;
    }

    if (pthread_cond_init(cond_ptr, &cond_attr))
    {
        perror("pthread_cond_init");
        return false;
    }

    if (pthread_condattr_destroy(&cond_attr))
    {
        perror("pthread_condattr_destroy");
        return false;
    }

    connector->response_available = false;

    return EXIT_SUCCESS;
}

void shm_controller_init(radio *radio_h, pthread_t *shm_tid)
{
    controller_conn *connector;

    if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)))
    {
        fprintf(stderr, "Connector SHM is already created!\nDestroying it and creating again.\n");
        shm_destroy(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
    }
    shm_create(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));

    connector = shm_attach(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
    connector_local = connector;

    initialize_connector(connector);

    pthread_create(shm_tid, NULL, process_radio_command_thread, (void *) connector);
}

void shm_controller_shutdown(pthread_t *shm_tid)
{
    controller_conn *connector = connector_local;

    pthread_cond_signal(&connector->cmd_condition);
    pthread_mutex_unlock(&connector->cmd_mutex);

    pthread_join(*shm_tid, NULL);
}
