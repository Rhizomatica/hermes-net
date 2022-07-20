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
 * Vara support routines
 */

/**
 * @file vara.h
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief VARA modem support functions
 *
 * All the specific code for supporting VARA.
 *
 */

#ifndef HAVE_VARA_H__
#define HAVE_VARA_H__

#include <stdint.h>
#include <pthread.h>

#include "uuardopd.h"

#ifdef __cplusplus
extern "C" {
#endif

// here we have the maximum number of messages when we stop sending and wait to get ack from other side (TODO)
#define MAX_VARA_BUFFER 8192
#define MAX_VARA_PACKET_SAFE 16384

bool initialize_modem_vara(rhizo_conn *connector);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_VARA__ */
