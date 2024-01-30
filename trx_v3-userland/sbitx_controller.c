/* HERMES sbitx controller
 *
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

#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>

#include "sbitx_alsa.h"
#include "sbitx_shm.h"
#include "sbitx_core.h"
#include "sbitx_websocket.h"
#include "sbitx_dsp.h"
#include "cfg_utils.h"

_Atomic bool shutdown_ = false;

void exit_radio(int sig)
{
    printf("Exiting...\n");
    shutdown_ = true;
}

int main(int argc, char* argv[])
{
    radio radio_h; // radio handler
    pthread_t cfg_tid; // configuration subsystem thread id
    pthread_t hw_tids[2]; // 2 hw thread ids user for IO
    pthread_t web_tid; // websocket thread id
    pthread_t shm_tid; // shared memory interface thread id
    pthread_t control_tid, radio_capture, radio_playback, loop_capture, loop_playback; // audio threads

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

   /* Call in order... cfg, hw, shm, sound, shutdown in reverse order */
   cfg_init(&radio_h, CFG_CORE_PATH, CFG_USER_PATH, &cfg_tid);

   hw_init(&radio_h, hw_tids);

   if (radio_h.enable_websocket)
       websocket_init(&radio_h, CFG_WEBSOCKET_PATH, &web_tid);

   if (radio_h.enable_shm_control)
       shm_controller_init(&radio_h, &shm_tid);

   dsp_init(&radio_h);
   sound_system_init(&radio_h, &control_tid, &radio_capture, &radio_playback, &loop_capture, &loop_playback);

   // the next call calls pthread_join(), so it blocks until shutdown == true
   hw_shutdown(&radio_h, hw_tids);
   cfg_shutdown(&radio_h, &cfg_tid);

   if (radio_h.enable_websocket)
       websocket_shutdown(&web_tid);

   if (radio_h.enable_shm_control)
       shm_controller_shutdown(&shm_tid);

   sound_system_shutdown(&radio_h, &control_tid, &radio_capture, &radio_playback, &loop_capture, &loop_playback);
   dsp_free(&radio_h);

   return EXIT_SUCCESS;

}
