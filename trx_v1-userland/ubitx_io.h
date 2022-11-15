/* ubitx_io
 * Copyright (C) 2022 Rhizomatica
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


#ifndef UBITX_IO_H_
#define UBITX_IO_H_

#include <stdint.h>
#include <stdbool.h>

#include "ubitx_controller.h"

// returns true is response was received
// response is copied to response... so pass a valid 5 bytes pointer
bool radio_cmd(controller_conn *connector, uint8_t *srv_cmd, uint8_t *response);


#endif // UBITX_IO_H_
