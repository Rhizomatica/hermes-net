/* sbitx_io
 * Copyright (C) 2023 Rhizomatica
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

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

#include "sbitx_io.h"
#include "../include/radio_cmds.h"

bool radio_cmd(controller_conn *connector, uint8_t *srv_cmd, uint8_t *response)
{
    bool ret_value = false;

    pthread_mutex_lock(&connector->response_mutex);
    pthread_mutex_lock(&connector->cmd_mutex);

    memcpy(connector->service_command, srv_cmd, 5);

    pthread_cond_signal(&connector->cmd_condition);
    pthread_mutex_unlock(&connector->cmd_mutex);

    connector->command_available = 1;

    if (srv_cmd[4] == CMD_RADIO_RESET)
        goto get_out;

    // we wait max of 20ms... worse case should be 1ms is software is running ok
    int tries = 0;
    while (connector->command_available == 1 && tries < 100)
    {
        usleep(200); // 0.2 ms
        tries++;
    }

    if (connector->command_available == 0)
    {
        memcpy(response, connector->response_service, 5);
        ret_value = true;
    }
    else
    {
        connector->command_available = 0;
        // fprintf(stderr, "Command NOT executed in 20ms! Dropping it!\n");
    }

 get_out:
    pthread_mutex_unlock(&connector->response_mutex);

    return ret_value;
}
