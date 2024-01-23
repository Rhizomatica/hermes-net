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

#include "cfg_utils.h"
#include "sbitx_core.h"
#include "shm_utils.h"
#include "sbitx_shm.h"

#include "radio_cmds.h"

extern _Atomic bool shutdown_;

static controller_conn *connector_local;

// local radio handle pointer used for simplifying the function prototypes
static radio *radio_h_shm;

void process_radio_command(uint8_t *cmd, uint8_t *response)
{
    uint32_t frequency = 0;
    uint8_t profile;

    radio *radio_h = radio_h_shm;
    memset(response, 0, 5);

    // here we split the upper 2 bits, with the profile number, and lower 6 bits, with the command
   switch(cmd[4] & 0x3f){

   case CMD_PTT_ON: // PTT On
       if (radio_h->swr_protection_enabled)
       {
           response[0] = CMD_ALERT_PROTECTION_ON;
       }
       else if (radio_h->txrx_state == IN_RX)
       {
           // TODO: sound_input(1);
           tr_switch(radio_h, IN_TX);
           response[0] = CMD_RESP_ACK;
       }
       else
       {
           response[0] = CMD_RESP_PTT_ON_NACK;
       }
       break;

   case CMD_PTT_OFF: // PTT OFF
       // tx was already shut at this point
       if (radio_h->swr_protection_enabled)
       {
           response[0] = CMD_ALERT_PROTECTION_ON;
       }
       else if (radio_h->txrx_state == IN_TX)
       {
           // TODO: sound_input(0);
           tr_switch(radio_h, IN_RX);
           response[0] = CMD_RESP_ACK;
       }
       else
       {
           response[0] = CMD_RESP_PTT_OFF_NACK;
       }
       break;

   case CMD_GET_TXRX_STATUS: // GET TX/RX STATUS
       if (radio_h->txrx_state == IN_TX)
           response[0] = CMD_RESP_GET_TXRX_INTX;
       else
           response[0] = CMD_RESP_GET_TXRX_INRX;
       break;

   case CMD_RESET_PROTECTION: // RESET PROTECTION
       response[0] = CMD_RESP_ACK;
       radio_h->swr_protection_enabled = false;
       radio_h->send_ws_update = true;
       break;

   case CMD_GET_PROTECTION_STATUS: // GET PROTECTION STATUS
       if (radio_h->swr_protection_enabled)
           response[0] = CMD_RESP_GET_PROTECTION_ON;
       else
           response[0] = CMD_RESP_GET_PROTECTION_OFF;
       break;

   case CMD_GET_BFO: // GET BFO
       response[0] = CMD_RESP_GET_BFO_ACK;
       memcpy(response+1, &radio_h->bfo_frequency, 4);
       break;

   case CMD_SET_BFO: // SET BFO
       response[0] = CMD_RESP_ACK;
       uint32_t bfo_freq;
       memcpy(&bfo_freq, cmd, 4);
       set_bfo(radio_h, bfo_freq);
       break;

   case CMD_GET_FWD: // GET FWD
       response[0] = CMD_RESP_GET_FWD_ACK;
       uint16_t fwdpower = (uint16_t) get_fwd_power(radio_h);
       memcpy(response+1, &fwdpower, 2);
       break;

   case CMD_GET_REF: // GET REF
       response[0] = CMD_RESP_GET_REF_ACK;
       uint16_t vswr = (uint16_t) get_swr(radio_h);
       memcpy(response+1, &vswr, 2);
       break;

   case CMD_GET_LED_STATUS: // GET LED STATUS
       if (radio_h->system_is_ok)
           response[0] = CMD_RESP_GET_LED_STATUS_ON;
       else
           response[0] = CMD_RESP_GET_LED_STATUS_OFF;
       break;

   case CMD_SET_LED_STATUS: // SET LED STATUS
       response[0] = CMD_RESP_ACK;
       radio_h->system_is_ok = cmd[0];
       break;

   case CMD_GET_CONNECTED_STATUS: // GET CONNECTED STATUS
       if (radio_h->system_is_connected)
           response[0] = CMD_RESP_GET_CONNECTED_STATUS_ON;
       else
           response[0] = CMD_RESP_GET_CONNECTED_STATUS_OFF;
       break;

   case CMD_SET_CONNECTED_STATUS: // SET CONNECTED STATUS
       response[0] = CMD_RESP_ACK;
       radio_h->system_is_connected = cmd[0];
       break;

   case CMD_GET_SERIAL: // GET SERIAL NUMBER
       response[0] = CMD_RESP_GET_SERIAL_ACK;
       memcpy(response+1, &radio_h->serial_number, 4);
       break;

   case CMD_SET_SERIAL: // SET SERIAL NUMBER
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->serial_number, cmd, 4);
       radio_h->cfg_core_dirty = true;
       break;

   case CMD_GET_STEPHZ: // CMD_GET_STEPHZ
       response[0] = CMD_RESP_GET_SERIAL_ACK;
       memcpy(response+1, &radio_h->step_size, 4);
       break;

   case CMD_SET_STEPHZ: // CMD_SET_STEPHZ
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->step_size, cmd, 4);
       radio_h->cfg_user_dirty = true;
       break;

   case CMD_GET_REF_THRESHOLD: // CMD_GET_REF_THRESHOLD
       response[0] = CMD_RESP_GET_REF_THRESHOLD_ACK;
       uint16_t reflected_threshold_g = (uint16_t) radio_h->reflected_threshold;
       memcpy(response+1, &reflected_threshold_g, 2);
       break;

   case CMD_SET_REF_THRESHOLD: // CMD_SET_REF_THRESHOLD
       response[0] = CMD_RESP_ACK;
       uint16_t reflected_threshold_s;
       memcpy(&reflected_threshold_s, cmd, 2);
       radio_h->reflected_threshold = (uint32_t) reflected_threshold_s;
       // cade?
       // radio_h->cfg_user_dirty = true;
       break;

   case CMD_GET_PROFILE: // CMD_GET_PROFILE
       response[0] = CMD_RESP_GET_PROFILE;
       response[1] = (uint8_t) radio_h->profile_active_idx;
       break;

   case CMD_SET_PROFILE: // CMD_SET_PROFILE
       response[0] = CMD_RESP_ACK;
       radio_h->profile_active_idx = (uint32_t) cmd[0];
       // TODO: we need a function to switch profile...
       break;

   case CMD_RADIO_RESET: // RADIO RESET
       response[0] = CMD_RESP_WRONG_COMMAND;
       shutdown_ = true;
       break;

   case CMD_GET_FREQ: // GET FREQUENCY
       response[0] = CMD_RESP_GET_FREQ_ACK;
       profile = cmd[4] >> 6;
       if (profile < radio_h->profiles_count)
           frequency = radio_h->profiles[profile].freq;
       memcpy(response+1, &frequency, 4);
       break;

   case CMD_SET_FREQ: // SET FREQUENCY
       response[0] = CMD_RESP_ACK;
       profile = cmd[4] >> 6;
       if (profile < radio_h->profiles_count)
       {
           memcpy(&frequency, cmd, 4);
           set_frequency(radio_h, frequency, profile);
       }
       break;

   case CMD_SET_MODE: // set mode
       response[0] = CMD_RESP_ACK;
       profile = cmd[4] >> 6;
       if (cmd[0] == 0x00 || cmd[0] == 0x03)
       {
           set_mode(radio_h, MODE_LSB, profile);
       }
       if (cmd[0] == 0x01)
       {
           set_mode(radio_h, MODE_LSB, profile);
       }
       if (cmd[0] == 0x04)
       {
           set_mode(radio_h, MODE_LSB, profile);
       }
       break;

   case CMD_GET_MODE: // GET SSB MODE
       profile = cmd[4] >> 6;
       if (radio_h->profiles[profile].mode == MODE_USB)
           response[0] = CMD_RESP_GET_MODE_USB;
       if (radio_h->profiles[profile].mode == MODE_LSB)
           response[0] = CMD_RESP_GET_MODE_LSB;
       if (radio_h->profiles[profile].mode == MODE_CW)
           response[0] = CMD_RESP_GET_MODE_CW;
       break;

#if 0
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

#endif
    default:
        response[0] = CMD_RESP_WRONG_COMMAND;
    }

}

// this is shm command thread
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
    radio_h_shm = radio_h;

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

    // we unblock the command thread so it can properly exit
    pthread_cond_signal(&connector->cmd_condition);
    pthread_mutex_unlock(&connector->cmd_mutex);

    pthread_join(*shm_tid, NULL);
}
