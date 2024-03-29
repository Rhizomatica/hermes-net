/* ubitx_io
 * Copyright (C) 2021 Rhizomatica
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

#include "ubitx_io.h"
#include "../include/radio_cmds.h"

bool radio_cmd(controller_conn *connector, uint8_t *srv_cmd, uint8_t *response)
{
    bool ret_value = false;

    pthread_mutex_lock(&connector->response_mutex);
    pthread_mutex_lock(&connector->cmd_mutex);

    memcpy(connector->service_command, srv_cmd, 5);

    // we clear any previous response not properly read, due to delayed response
    if (connector->response_available == 1)
        connector->response_available = 0;

    pthread_cond_signal(&connector->cmd_condition);
    pthread_mutex_unlock(&connector->cmd_mutex);

    if (srv_cmd[4] == CMD_RADIO_RESET)
        goto get_out;

    // ~30 ms max wait
    int tries = 0;
    while (connector->response_available == 0 && tries < 30)
    {
        usleep(1000); // 1 ms
        tries++;
    }

    if (connector->response_available == 1)
    {
      memcpy(response, connector->response_service, 5);
      connector->response_available = 0;
      ret_value = true;
    }

 get_out:
    pthread_mutex_unlock(&connector->response_mutex);

    return ret_value;
}
