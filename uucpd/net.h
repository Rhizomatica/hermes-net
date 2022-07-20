/* Rhizo-connector: A connector to different HF modems
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
 * Network related functions
 */

/**
 * @file net.h
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief File containing network related functions
 *
 * For the sake of reusable and clean code, some network related functions.
 *
 */

#ifndef HAVE_NET_H__
#define HAVE_NET_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_BLOCK 512

bool tcp_connect(char *ip, int port, int *sockt);
bool tcp_read(int sockt, uint8_t *buffer, size_t rx_size);
bool tcp_write(int sockt, uint8_t *buffer, size_t rx_size);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_NET_H__ */
