/* Rhizo-uuardop: Tools to integrate Ardop to UUCP
 * Copyright (C) 2019 Rhizomatica
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
 * Routines to call uucico when receiving a call
 */

/**
 * @file call_uucico.c
 * @author Rafael Diniz
 * @date 26 Jul 2019
 * @brief Routines to call uucico when receiving a call
 *
 * Code to call uucico
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
#include <dirent.h>
#include <sys/inotify.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <grp.h>

#include "call_uucico.h"

bool call_uucico(rhizo_conn *connector){

    fprintf(stderr, "call_uucico: Sending signal to start uucico!\n");

    if (connector->uucico_active == true)
        fprintf(stderr, "Warning: trying to activate already active uucico!\n");
    else
        connector->uucico_active = true;

    return true;
}

void *uucico_thread(void *conn){
    rhizo_conn *connector = conn;
    pid_t pid;
    int st;

    // set some file descriptors as in in.uucpd...
    // use 2 pipe() to create the fds for I/O
    // fork()
    // in the parent, call wait to wait for uucico (in a new thread?)
    // in the child, remap the fds to 0, 1 (and 2?) 
    // in the child, call execl() (or execlp() (uucico -l)

    while (connector->shutdown == false)
    {
        while (connector->uucico_active == false)
            usleep(100000); // 0.1s

        fprintf(stderr, "uucico_thread: session started!\n");

        // parent write to child
        pipe(connector->pipefd1); // pipe[0] is read, pipe[1] is write
        // child write to parent
        pipe(connector->pipefd2);

        pthread_t tid1;
        pthread_create(&tid1, NULL, uucico_read_thread, (void *) connector);

        pthread_t tid2;
        pthread_create(&tid2, NULL, uucico_write_thread, (void *) connector);

        // parent
        if ((pid = fork()) != 0)
        {
            if (pid < 0) {
                fprintf(stderr, "fork() error.\n");
                return NULL;
            }

            close(connector->pipefd1[0]);
            close(connector->pipefd2[1]);

            // pthread_create the two threads which does the job of reading / writing from/to buffers and fds...

            while(wait(&st) != pid);
            if ( WIFEXITED(st) ){
                fprintf(stderr, "uucico child exec exited with status = %d\n", WEXITSTATUS(st));
                // uucico ended!
                // we should disconnect here!
            }

            close(connector->pipefd1[1]);
            close(connector->pipefd2[0]);

//            fprintf(stderr, "uucico before join\n");

            pthread_join(tid1, NULL);
            pthread_join(tid2, NULL);

//            fprintf(stderr, "uucico after join\n");

            // usleep(2000000); // 2s for the system to cool down
            connector->clean_buffers = true;

            connector->send_break = true;
            connector->uucico_active = false;

            fprintf(stderr, "uucico_thread: session ended!\n");
            continue;
        }

        // this is the child (uucico)
        close(0);
        close(1);
        close(2);

        dup2(connector->pipefd1[0], 0);
        dup2(connector->pipefd2[1], 1);
        dup2(connector->pipefd2[1], 2); // is this correct?

        close(connector->pipefd1[0]);
        close(connector->pipefd2[1]);
        close(connector->pipefd1[1]); // closing write pipefd1 (child reads from parent)
        close(connector->pipefd2[0]); // closing read pipefd2 (child writes to parent)

#if 0 // lets run all as root
        char pwd[] = "/var/spool/uucp"; // uucp home
        if (chdir(pwd) != 0) {
            perror(pwd);
            exit(1);
        }
        gid_t gid = 10; // uucp gid
        if (setgid(gid) != 0) {
            perror("setgid");
            exit(1);
        }
        char user[] = "uucp";
        if (initgroups(user, gid) < 0) {
            perror("initgroups");
            exit(1);
        }
        uid_t uid = 10; // uucp uid
        if (setuid(uid) != 0) {
            perror("setuid");
            exit(1);
        }
#endif
        char shell[] = "/usr/sbin/uucico";

//        setenv("LOGNAME", user, 1);
//        setenv("USER", user, 1);
        setenv("SHELL", shell, 1);
        setenv("TERM", "dumb", 1);

        if (connector->ask_login == true)
            if(connector->ask_uucp_msg == true)
                execl(shell, shell, "-l","-m", NULL);
            else
                execl(shell, shell, "-l", NULL);
        else
            if(connector->ask_uucp_msg == true)
                execl(shell, shell, "-m", NULL);
            else
                execl(shell, shell, NULL);

        perror(shell);

        _exit(EXIT_SUCCESS);
    }

    return NULL;
}

void *uucico_read_thread(void *conn)
{
    bool running = true;
    rhizo_conn *connector = (rhizo_conn *) conn;
    int num_read = 0;
    int bytes_pipe = 0;
    uint8_t buffer[BUFFER_SIZE];

    while(running)
    {
        ioctl(connector->pipefd2[0], FIONREAD, &bytes_pipe);
//        fprintf(stderr, "trying to read from uucico %d bytes!\n", bytes_pipe);

        if (bytes_pipe > BUFFER_SIZE)
            bytes_pipe = BUFFER_SIZE;
        if (bytes_pipe <= 0)
            bytes_pipe = 1; // so we block in read() in case of no data to read

        // workaround to make protocol 'y' work better
        while (circular_buf_size(connector->in_buffer) > BUFFER_SIZE/2)
        {
            bytes_pipe = 1; // slow down...
            usleep(100000); // 0.1s
        }

        num_read = read(connector->pipefd2[0], buffer, bytes_pipe);

        if (num_read > 0)
        {
            while (circular_buf_free_size(connector->in_buffer) < num_read)
                usleep(20000);
            circular_buf_put_range(connector->in_buffer, buffer, num_read);
        }
        if (num_read == 0)
        {
            fprintf(stderr, "uucico_read_thread: read == 0\n");
            running = false;
        }
        if (num_read == -1)
        {
            fprintf(stderr, "uucico_read_thread: read() error! error no: %d\n",errno);
            running = false;
        }
    }

    connector->session_counter_read++;

    return NULL;
}


void *uucico_write_thread(void *conn) {
    bool running = true;
    rhizo_conn *connector = (rhizo_conn *) conn;
    int bytes_to_read = 0;
    int num_written = 0;
    uint8_t buffer[BUFFER_SIZE];

    while (running)
    {
        bytes_to_read = circular_buf_size(connector->out_buffer);
        if (bytes_to_read == 0)
        { // we spinlock here
            usleep(100000); // 0.1s
            if (connector->session_counter_read > connector->session_counter_write)
                running = false;
            continue;
        }

        if (bytes_to_read > BUFFER_SIZE)
            bytes_to_read = BUFFER_SIZE;

        circular_buf_get_range(connector->out_buffer, buffer, bytes_to_read);

        num_written = write(connector->pipefd1[1], buffer, bytes_to_read);
        if (num_written == 0)
        {
            fprintf(stderr, "pipe_write_thread: write == 0\n");
            running = false;
        }
        if (num_written == -1)
        {
            running = false;
            if (errno == EPIPE)
            {
                fprintf(stderr, "uucico_write_thread: write() EPIPE!\n");
            }
            else
            {
                fprintf(stderr, "uucico_write_thread: write() error no: %d\n", errno);
            }
        }
    }

    connector->session_counter_write++;

    return NULL;
}
