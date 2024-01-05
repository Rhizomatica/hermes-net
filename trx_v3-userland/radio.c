/* hermes-net trxv3-userland
 *
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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "sbitx_core.h"
#include "cfg_utils.h"

_Atomic bool shutdown = false;

void exit_radio(int sig)
{
    shutdown = true;

}

int main(int argc, char* argv[])
{
    radio radio_h; // mais radio handler

    signal(SIGINT, exit_radio);

    if (argc != 1)
    {
        printf("Usage:\n%s\n", argv[0]);
        return EXIT_FAILURE;
    }


    memset(&radio_h, 0, sizeof(radio));
    init_config_core(&radio_h, "config/test-core.ini");
    init_config_user(&radio_h, "config/test-user.ini");

    int rc = iniparser_set(radio_h.cfg_core, "main:serial_number", "666");
    if (rc != 0)
        printf("Error modifying config file\n");

    // start config file writer thread
    pthread_t config_tid;
    pthread_create( &config_tid, NULL, config_thread, (void *) &radio_h);

    hw_init(&radio_h);

    // set rt_prio here?
    while(!shutdown)
    {
        sleep(1);
    }

    pthread_join(config_tid, NULL);

    hw_shutdown(&radio_h);

    close_config_core(&radio_h);
    close_config_user(&radio_h);

    return EXIT_SUCCESS;

}
