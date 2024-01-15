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

#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>

#include "sbitx_core.h"
#include "cfg_utils.h"

_Atomic bool shutdown = false;

void exit_radio(int sig)
{
    printf("Exiting...\n");
    shutdown = true;

}

int main(int argc, char* argv[])
{
    radio radio_h; // radio handler
    pthread_t cfg_tid; // configuration subsystem thread id
    pthread_t hw_tid; // hw io thread id

   if (argc > 3)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s\n%s -c [cpu_nr]\n", argv[0], argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -c [cpu_nr]                Run on CPU [cpu_br].\n");
        fprintf(stderr, " -h                         Prints this help.\n");
        return EXIT_FAILURE;
    }

   int cpu_nr = -1;
   int opt;
   while ((opt = getopt(argc, argv, "hc:")) != -1)
   {
       switch (opt)
       {
       case 'c':
           if(optarg)
               cpu_nr = atoi(optarg);
           break;
       case 'h':
       default:
           goto manual;
       }
   }

   // our shutdown handling...
   signal(SIGINT, exit_radio);
   signal(SIGQUIT, exit_radio);
   signal(SIGTERM, exit_radio);

    // Catch SIGPIPE
   signal(SIGPIPE, SIG_IGN); // and ignores SIGPIPE...

   memset(&radio_h, 0, sizeof(radio));

   if (cpu_nr != -1)
   {
       cpu_set_t mask;
       CPU_ZERO(&mask);
       CPU_SET(cpu_nr, &mask);
       sched_setaffinity(0, sizeof(mask), &mask);
       printf("RUNNING ON CPU Nr %d\n", sched_getcpu());
   }

   /* Call in order... cfg, hw, etc */
   cfg_init(&radio_h, CFG_CORE_PATH, CFG_USER_PATH, &cfg_tid);
   hw_init(&radio_h, &hw_tid);

   // for testing purposes...
#if 0
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
#endif
   // this call pthread_join(), so it blocks below, until shutdown == true
   hw_shutdown(&radio_h, &hw_tid);
   cfg_shutdown(&radio_h, &cfg_tid);

   return EXIT_SUCCESS;

}