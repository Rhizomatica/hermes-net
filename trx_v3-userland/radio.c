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
    // read the config file

// //    strcpy(radio_h->i2c_device, "/dev/i2c-22");
// //    radio_h->bfo_frequency = 40035000;
// //    radio_h->bridge_compensation = 100;

    hw_init(&radio_h);

    // set_frequency(&radio_h, frequency);


    // set rt_prio here?
    while(1)
    {


    }
    
    hw_shutdown(&radio_h);

}
