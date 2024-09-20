/* Rhizo-uuhf: Tools to integrate HF TNCs to UUCP
 * Copyright (C) 2020-2021 Rhizomatica
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

#ifndef HAVE_SERIAL_H__
#define HAVE_SERIAL_H__

#include <stdbool.h>
#include <stdint.h>

#define MAX_MODEM_PATH 4096
#define MAX_BUF_SIZE 4096

#define RADIO_TYPE_ICOM 0
#define RADIO_TYPE_UBITX 1
#define RADIO_TYPE_SHM 2

void key_on(int serial_fd, int radio_type);
void key_off(int serial_fd, int radio_type);

void connected_led_on(int serial_fd, int radio_type);
void connected_led_off(int serial_fd, int radio_type);

void sys_led_on(int serial_fd, int radio_type);
void sys_led_off(int serial_fd, int radio_type);

int open_serial_port(char *ttyport);
void set_fixed_baudrate(char *baudname, int target_fd);

void modem_snr(int32_t snr);
void modem_bitrate(uint32_t bitrate);


#endif // HAVE_SERIAL_H__
