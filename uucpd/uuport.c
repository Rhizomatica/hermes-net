/* UUCPD: Tools which integrate HF to UUCP
 * Copyright (C) 2019-2015 Rhizomatica
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

/**
 * @file uuport.c
 * @author Rafael Diniz
 * @date 14 Aug 2019
 * @brief UUCP port
 *
 * UUPORT main C file.
 *
 */

#include <sys/ioctl.h>
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
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>

#include "uucpd.h"
#include "uuport.h"
#include "shm.h"
#include "circular_buffer.h"

rhizo_conn *connector_aux = NULL;
FILE *log_fd;
atomic_bool running_read;
atomic_bool running_write;

/* 40s timeout, counted in 0.1s units to reduce connect/IO latency. */
#define TIMEOUT 400

void *read_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_to_read = 0;
    int bytes_written = 0;
    int timeout_counter = TIMEOUT;

    running_read = true;
    while (running_read && (connector->shutdown == false))
    {
        if (connector->connected == false)
        {
            usleep(100000); // 0.1s
            timeout_counter--;
            if (timeout_counter == 0)
            {
                running_read = false;
            }
        }
        else{
            timeout_counter = TIMEOUT;
        }

        bytes_to_read = circular_buf_size(connector->out_buffer_p);

        if (bytes_to_read == 0)
        { // we spinlock here
            usleep(10000); // 10ms
            continue;
        }

        if (bytes_to_read > BUFFER_SIZE)
            bytes_to_read = BUFFER_SIZE;

        circular_buf_get_range(connector->out_buffer_p, buffer, bytes_to_read);

        bytes_written = write(1, buffer, bytes_to_read);

//        fprintf(log_fd, "uuport: %d bytes written to uucico\n", bytes_written);

        if (bytes_written != bytes_to_read)
        {
            fprintf(log_fd, "read_thread: bytes_written: %d != bytes_read: %d.\n", bytes_written, bytes_to_read);
            running_read = false;
            continue;
        }
        if (bytes_written == -1)
        {
            fprintf(log_fd, "read_thread: write() error no: %d\n", errno);
            running_read = false;
            continue;
        }

    }
//    connector->session_counter_read++;

    return NULL;

}

void *write_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_to_read = 0;
    int bytes_read = 0;

    running_write = true;
    while(running_write && (connector->shutdown == false))
    {
        if (connector->clean_buffers == true)
        {
            running_write = false;
            continue;
        }
        // workaround to make protocol 'y' work better
        if (circular_buf_size(connector->in_buffer_p) > BUFFER_SIZE / 2)
        {
            usleep(10000); // 10ms
            bytes_to_read = 1; // slow down...
        }
        else
        {
            bytes_to_read = 512; // protocol 'y' packet size
        }

        bytes_read = read(0, buffer, bytes_to_read);

        // fprintf(log_fd, "uuport: %d bytes read from uucico\n", bytes_read);

        if (bytes_read == -1)
        {
            fprintf(log_fd, "write_thread: Error in read(), errno: %d\n", errno);
            running_write = false;
            continue;
        }
        if (bytes_read == 0)
        {
            fprintf(log_fd, "write_thread: read() returned 0\n");
            running_write = false;
            continue;
        }

        while (circular_buf_free_size(connector->in_buffer_p) < bytes_read)
        {
            fprintf(log_fd, "Buffer full!\n");
            usleep(10000);
        }
        circular_buf_put_range(connector->in_buffer_p, buffer, bytes_read);
    }

    running_read = false;
//    connector->session_counter_write++;

    return NULL;
}

void finish(int s){

    if (s == SIGPIPE){
        fprintf(log_fd, "\nSIGPIPE: Doing nothing.\n");
        return;
    }

    if (s == SIGINT)
        fprintf(log_fd, "\nSIGINT: Exiting.\n");

    if (s == SIGTERM)
        fprintf(log_fd, "\nSIGTERM: Exiting.\n");

    if (s == SIGQUIT)
        fprintf(log_fd, "\nSIGQUIT: Exiting.\n");

    if (s == SIGHUP)
    {
        fprintf(log_fd, "\nSIGHUP: running shutdown...\n");
        running_write = false;
        running_read = false;
        fflush(log_fd);
        sleep(1);
    }

    // some house keeping here?
    if (connector_aux)
        connector_aux->clean_buffers = true;

    fclose(log_fd);
    exit(EXIT_SUCCESS);
}


int main (int argc, char *argv[])
{
    rhizo_conn *connector = NULL;

    char log_file[BUFFER_SIZE];
    log_file[0] = 0;

    char remote_system[32];
    remote_system[0] = 0;

    signal (SIGINT, finish);
    signal (SIGTERM, finish);
    signal (SIGQUIT, finish);
    signal (SIGHUP, finish);
    signal (SIGPIPE, finish);

    if (argc < 1)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s -l logfile\n", argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -e logfile.txt               Log file (default is stderr).\n");
        fprintf(stderr, " -c system_name               Name of the remote system (default is don't change).\n");
        fprintf(stderr, " -h                           Prints this help.\n");
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hc:e:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            goto manual;
            break;
        case 'e':
            strcpy(log_file, optarg);
            break;
        case 'c':
            strcpy(remote_system, optarg);
            break;
        default:
            goto manual;
        }
    }

    if (shm_is_created(SYSV_SHM_KEY_STR, sizeof(rhizo_conn)) == false)
    {
        fprintf(stderr, "Connector SHM not created. Is uuardopd running?\n");
        return EXIT_FAILURE;
    }
    connector = shm_attach(SYSV_SHM_KEY_STR, sizeof(rhizo_conn));

    if (connector->shutdown == true)
    {
        fprintf(stderr, "uuardopd is in shutdown state. Exiting.\n");
        return EXIT_FAILURE;
    }

    connector->in_buffer_p = circular_buf_connect_shm(INTERNAL_BUFFER_SIZE, SYSV_SHM_KEY_IB);
    connector->out_buffer_p = circular_buf_connect_shm(INTERNAL_BUFFER_SIZE, SYSV_SHM_KEY_OB);

    // for signal() use
    connector_aux = connector;

    if (log_file[0])
    {
        log_fd = fopen(log_file, "a");
        if (log_fd == NULL)
        {
            fprintf(stderr, "Log file could not be opened: %s\n", log_file);
            fprintf(stderr, "Reverting to stderr log.\n");
            log_fd = stderr;
        }
    }
    else
    {
        log_fd = stderr;
    }

    if (remote_system[0])
    {
        strcpy(connector->remote_call_sign, remote_system);
    }

    pthread_t tid;
    pthread_create(&tid, NULL, write_thread, (void *) connector);

    read_thread(connector);

    connector->clean_buffers = true;

    // workaround... as write_thread blocks in fd 0...
    fclose(log_fd);
    return EXIT_SUCCESS;

    // correct should be this...
    pthread_join(tid, NULL);
    return EXIT_SUCCESS;
}
