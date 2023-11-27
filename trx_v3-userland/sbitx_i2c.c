/* sBitx core
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

#include "sbitx_core.h"
#include "sbitx_i2c.h"


int i2c_read_pwr_levels(radio *radio_h, uint8_t *response)
{
    int ret_value = -1;

    pthread_mutex_lock(&radio_h->i2c_mutex);

    if (ioctl(radio_h->i2c_bus, I2C_SLAVE, ATTINY85_I2C) == 0)
        ret_value = read(radio_h->i2c_bus, response, 4);

    pthread_mutex_unlock(&radio_h->i2c_mutex);

    if (ret_value != 4)
        ret_value = -1;

    return ret_value;
}

void i2c_write_si5351(radio *radio_h, uint8_t reg, uint8_t val){

    pthread_mutex_lock(&radio_h->i2c_mutex);

    if (ioctl(radio_h->i2c_bus, I2C_SLAVE, SI5351_I2C) == 0)
        i2c_smbus_write_byte_data(radio_h->i2c_bus, reg, val);

    pthread_mutex_unlock(&radio_h->i2c_mutex);
}

bool i2c_open(radio *radio_h)
{
    pthread_mutex_init(&radio_h->i2c_mutex, NULL);

    radio_h->i2c_bus = open(radio_h->i2c_device, O_RDWR);

    if (radio_h->i2c_bus < 0)
        return false;

    return true;
}

bool i2c_close(radio *radio_h)
{
    if (close(radio_h->i2c_bus) >= 0)
        return true;
    else
        return false;
}
