/* ubitx_controller
 * Copyright (C) 2023 Rhizomatica
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
#include <glib.h>

#include "sbitx_controller.h"
#include "../include/radio_cmds.h"

#include "shm.h"

static bool running;

extern void g_main_loop_quit ();
extern GMainLoop *loop_g;

extern void processCATCommand(uint8_t *cmd, uint8_t *response);
extern void tx_off();

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    tx_off();

    running = false;
    g_main_loop_quit (loop_g);
    sleep(1);

    // do some house cleaning here.... or somewhere else?
    exit(EXIT_SUCCESS);
}


int cat_tx(void *arg)
{
    controller_conn *conn = arg;

    pthread_mutex_lock(&conn->cmd_mutex);

    while(running)
    {

        pthread_cond_wait(&conn->cmd_condition, &conn->cmd_mutex);

        processCATCommand(conn->service_command, conn->response_service);

        if (conn->service_command[4] == CMD_RADIO_RESET)
        {
            running = false;
            pthread_mutex_unlock(&conn->cmd_mutex);
            fprintf(stderr,"\nReset command. Exiting\n");
            exit(EXIT_SUCCESS);
        }
        conn->response_available = true;

    }

    pthread_mutex_unlock(&conn->cmd_mutex);

    fprintf(stderr,"Sent to the radio:  0x%hhx\n", conn->service_command[4]);

    return EXIT_SUCCESS;
}

bool initialize_message(controller_conn *connector)
{
    // init mutexes
    pthread_mutex_t *mutex_ptr = (pthread_mutex_t *) & connector->cmd_mutex;

    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr)) {
        perror("pthread_mutexattr_init");
        return false;
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) {
        perror("pthread_mutexattr_setpshared");
        return false;
    }

    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST)){
        perror("pthread_mutexattr_setrobust");
        return false;
    }

    if (pthread_mutex_init(mutex_ptr, &attr)) {
        perror("pthread_mutex_init");
        return false;
    }

    mutex_ptr = (pthread_mutex_t *) & connector->response_mutex;

    if (pthread_mutex_init(mutex_ptr, &attr)) {
        perror("pthread_mutex_init");
        return false;
    }

    if (pthread_mutexattr_destroy(&attr)) {
        perror("pthread_mutexattr_destroy");
        return false;
    }

    // init the cond
    pthread_cond_t *cond_ptr = (pthread_cond_t *) & connector->cmd_condition;
    pthread_condattr_t cond_attr;
    if (pthread_condattr_init(&cond_attr)) {
        perror("pthread_condattr_init");
        return false;
    }
    if (pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED)) {
        perror("pthread_condattr_setpshared");
        return false;
    }

    if (pthread_cond_init(cond_ptr, &cond_attr)) {
        perror("pthread_cond_init");
        return false;
    }

    if (pthread_condattr_destroy(&cond_attr)) {
        perror("pthread_condattr_destroy");
        return false;
    }

    connector->response_available = false;

    return EXIT_SUCCESS;
}

void sbitx_controller()
{
    controller_conn *connector;

    if (shm_is_created(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn)))
    {
        fprintf(stderr, "Connector SHM is already created!\nDestroying it and creating again.\n");
        shm_destroy(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));
    }
    shm_create(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));

    connector = shm_attach(SYSV_SHM_CONTROLLER_KEY_STR, sizeof(controller_conn));

    initialize_message(connector);


    signal (SIGINT, finish);
    signal (SIGQUIT, finish);
    signal (SIGTERM, finish);

    running = true;

    thrd_t cat_thread;
    thrd_create(&cat_thread, cat_tx, connector);
}
