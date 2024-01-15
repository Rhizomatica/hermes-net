/* sBitx core sample application
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

/* This sample application demonstrate how to use some of the sBitx core functions */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>


#include "sbitx_core.h"

int shutdown = 0;

void exit_ptt(int sig)
{
    shutdown = 1;
}

int main(int argc, char *argv[])
{
    radio radio_h;

    signal(SIGINT, exit_ptt);

    if (argc != 2)
    {
        printf("Usage:\n%s <frequency in Hz>\n", argv[0]);
        return EXIT_FAILURE;
    }
    uint32_t frequency = atoi(argv[1]);

    // these are mandatory fields to be filled before hw_init()
    memset(&radio_h, 0, sizeof(radio));
    strcpy(radio_h.i2c_device, "/dev/i2c-22");
    radio_h.bfo_frequency = 40035000;
    radio_h.bridge_compensation = 100;


    hw_init(&radio_h);

    set_frequency(&radio_h, frequency);

    while(!shutdown)
    {
        // clean screen
        printf("\e[1;1H\e[2J");
        printf("\n");
        printf("volume_ticks: %d\n", radio_h.volume_ticks);
        printf("tuning_ticks: %d\n", radio_h.tuning_ticks);
        printf("knob A pressed: %u\n", radio_h.knob_a_pressed);
        printf("knob B pressed: %u\n", radio_h.knob_b_pressed);
        printf("PTT: %s\n", radio_h.key_down ? "DOWN" : "UP" );
        printf("DASH: %s\n", radio_h.dash_down ? "DOWN" : "UP" );

        if (radio_h.key_down)
            tr_switch(&radio_h, IN_TX);
        else
            tr_switch(&radio_h, IN_RX);

        printf("TR_SWITCH: %s\n", (radio_h.txrx_state == IN_TX) ? "TX" : "RX" );

        if (radio_h.txrx_state == IN_TX)
        {
            if (update_power_measurements(&radio_h))
            {
                printf("   FWD PWR: %.1f   REF PWR: %.1f   SWR: %.1f\n",
                       (float) get_fwd_power(&radio_h) / 10,
                       (float) get_ref_power(&radio_h) / 10,
                       (float) get_swr(&radio_h) / 10);
            }
            else
            {
                printf("   FWD PWR: ERROR   REF PWR: ERROR   SWR: ERROR     \r");
            }
        }

        usleep(50000);// 50 ms
    }

    tr_switch(&radio_h, IN_RX);
    printf("PTT OFF. Exiting.\n");

    hw_shutdown(&radio_h);

    return EXIT_SUCCESS;
}
