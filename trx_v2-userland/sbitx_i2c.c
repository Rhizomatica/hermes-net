/* sBitx controller
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "sbitx_i2c.h"

static int bus;
static pthread_mutex_t i2c_mutex;

int read_pwr_levels(uint8_t *response)
{
    pthread_mutex_lock(&i2c_mutex);

    int ret_value = -1;
    if (ioctl(bus, I2C_SLAVE, ATTINY85_I2C) == 0)
        ret_value = i2c_smbus_read_i2c_block_data(bus, 0, 4, response);

    pthread_mutex_unlock(&i2c_mutex);

    return ret_value;
}

void i2cSendRegister(uint8_t reg, uint8_t val){

    pthread_mutex_lock(&i2c_mutex);

    if (ioctl(bus, I2C_SLAVE, SI5351_I2C) == 0)
        i2c_smbus_write_byte_data(bus, reg, val);

    pthread_mutex_unlock(&i2c_mutex);
}

bool open_i2c(char *device)
{
    pthread_mutex_init(&i2c_mutex, NULL);
    bus = open(device, O_RDWR);
    if (bus < 0)
        return false; // failure
    return true; // all good
}

bool close_i2c(char *device)
{
    if (close(bus) >= 0)
        return true; // all good
    else
        return false; // failure
}
