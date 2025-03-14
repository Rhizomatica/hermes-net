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
#include "sbitx_io.h"

#include "radio_cmds.h"

extern _Atomic bool shutdown_;

controller_conn *connector_local;

extern bool timer_reset;
extern time_t timeout_counter;

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

   case CMD_TIMEOUT_RESET: // RESET TIMEOUT
       response[0] = CMD_RESP_ACK;
       timer_reset = true;
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
       if (!cmd[0])
       {
           // we cleanup the message buffer at disconnection
           connector_local->message_available = true;
           connector_local->message[0] = 0;
       }
       radio_h->system_is_connected = cmd[0];
       break;

   case CMD_GET_SERIAL: // GET SERIAL NUMBER
       response[0] = CMD_RESP_GET_SERIAL_ACK;
       memcpy(response+1, &radio_h->serial_number, 4);
       break;

   case CMD_SET_SERIAL: // SET SERIAL NUMBER
       response[0] = CMD_RESP_ACK;
       set_serial(radio_h, *(uint32_t *)cmd);
       break;

   case CMD_GET_STEPHZ: // CMD_GET_STEPHZ
       response[0] = CMD_RESP_GET_STEPHZ_ACK;
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
       set_reflected_threshold(radio_h, (uint32_t) reflected_threshold_s);
       break;

   case CMD_GET_PROFILE: // CMD_GET_PROFILE
       response[0] = CMD_RESP_GET_PROFILE;
       response[1] = (uint8_t) radio_h->profile_active_idx;
       break;

   case CMD_SET_PROFILE: // CMD_SET_PROFILE
       response[0] = CMD_RESP_ACK;
       uint32_t prof_set = (uint32_t) cmd[0];
       if (prof_set < radio_h->profiles_count)
           set_profile(radio_h, prof_set);
       break;

   case CMD_RADIO_RESET: // RADIO RESET
       response[0] = CMD_RESP_WRONG_COMMAND; // we just exit here... no point of response
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
           set_mode(radio_h, MODE_USB, profile);
       }
       if (cmd[0] == 0x04)
       {
           set_mode(radio_h, MODE_CW, profile);
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

    case CMD_GET_VOLUME: // GET_VOLUME
        response[0] = CMD_RESP_GET_VOLUME_ACK;
        profile = cmd[4] >> 6;
       if (profile < radio_h->profiles_count)
           memcpy(response+1, &radio_h->profiles[profile].speaker_level, 4);
        break;

    case CMD_SET_VOLUME: // SET_VOLUME
        response[0] = CMD_RESP_ACK;
        profile = cmd[4] >> 6;
        uint32_t speaker_level;
        memcpy(&speaker_level, cmd, 4);
        if (speaker_level < 0)
            speaker_level = 0;
        if (speaker_level > 100)
            speaker_level = 100;
        set_speaker_volume(radio_h, speaker_level, profile);
        break;

    case CMD_GET_TIMEOUT: // GET_TIMEOUT
        response[0] = CMD_RESP_GET_TIMEOUT_ACK;
        memcpy(response+1, &radio_h->profile_timeout, 4);
        break;

    case CMD_SET_TIMEOUT: // SET_TIMEOUT
        response[0] = CMD_RESP_ACK;
        set_profile_timeout(radio_h, *(int32_t *) cmd);
        break;

   case CMD_GET_TONE: // GET_TONE
       response[0] = CMD_RESP_GET_TONE_ACK;
       memcpy(response+1, &radio_h->tone_generation, 1);
       break;

   case CMD_SET_TONE: // SET_TONE
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->tone_generation, cmd, 1);
       break;

   case CMD_GET_BITRATE: // CMD_GET_BITRATE
       response[0] = CMD_RESP_GET_BITRATE;
       memcpy(response+1, &radio_h->bitrate, 4);
       break;

   case CMD_SET_BITRATE: // CMD_SET_BITRATE
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->bitrate, cmd, 4);
       break;

   case CMD_GET_SNR: // CMD_GET_SNR
       response[0] = CMD_RESP_GET_SNR;
       memcpy(response+1, &radio_h->snr, 4);
       break;

   case CMD_SET_SNR: // CMD_SET_SNR
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->snr, cmd, 4);
       break;

   case CMD_SET_BYTES_RX:
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->bytes_received, cmd, 4);
       break;

   case CMD_GET_BYTES_RX:
       response[0] = CMD_RESP_GET_BYTES_RX;
       memcpy(response+1, &radio_h->bytes_received, 4);
       break;

   case CMD_SET_BYTES_TX:
       response[0] = CMD_RESP_ACK;
       memcpy(&radio_h->bytes_transmitted, cmd, 4);
       break;

   case CMD_GET_BYTES_TX:
       response[0] = CMD_RESP_GET_BYTES_TX;
       memcpy(response+1, &radio_h->bytes_transmitted, 4);
       break;


// TODO: finish implementig the remaining commands
#if 0
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
    connector->message_available = false;

    return EXIT_SUCCESS;
}

void shm_controller_init(radio *radio_h, pthread_t *shm_tid)
{
    controller_conn *connector;
    radio_h_shm = radio_h;

    if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)))
    {
        fprintf(stderr, "Connector SHM is already created, Destroying it and creating again.\n");
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
