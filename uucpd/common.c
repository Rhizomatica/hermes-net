/* Rhizo-HF-connector: A connector to different HF modems
 * Copyright (C) 2018 Rhizomatica
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
 * Functions used by any type of modem
 */

/**
 * @file common.c
 * @author Rafael Diniz
 * @date 09 May 2018
 * @brief Common functions used by any type of modem
 *
 * Common functions used by any type of modem.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "uuardopd.h"
#include "common.h"


bool inotify_wait(char *file_name){
    struct stat   buffer;
    if (stat(file_name, &buffer) != 0)
    {
        if (mkfifo(file_name, S_IRWXU | S_IRWXG | S_IRWXO) != 0){
            fprintf(stderr, "Failed to create fifo: %s\n", file_name);
            return false;
        }
        chown(file_name, 10, 20);
    }

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))
    int length, i = 0;
    int fd;
    int wd;
    char buffer_inot[BUF_LEN];

    fd = inotify_init();

    if (fd < 0) {
        perror("inotify_init");
    }

    wd = inotify_add_watch(fd, file_name, IN_CREATE | IN_DELETE);

    length = read(fd, buffer_inot, BUF_LEN);

    if (length < 0) {
        perror("read");
    }

    i = 0;
    while (i < length) {
        struct inotify_event *event = (struct inotify_event *) &buffer_inot[i];
        i += EVENT_SIZE + event->len;
//        fprintf(stderr, "Got some unnamed event!\n");
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    sleep(1);

    return true;
}
