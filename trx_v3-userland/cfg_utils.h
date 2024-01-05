/* hermes-net trxv3-userland
 *
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

#ifndef CFG_UTILS_H_
#define CFG_UTILS_H_

#include <stdbool.h>

#include "sbitx_core.h"

// returns true if successful
bool init_config_core(radio *radio_h, char *ini_name);
bool init_config_user(radio *radio_h, char *ini_name);

bool write_config_core(radio *radio_h, char *ini_name);
bool write_config_user(radio *radio_h, char *ini_name);

bool close_config_core(radio *radio_h);
bool close_config_user(radio *radio_h);

// thread to write the configuration when dirty bit is set
void *config_thread(void *radio_h_v);


#endif // CFG_UTILS_H_
