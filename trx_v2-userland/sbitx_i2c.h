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

#ifndef SBITX_I2C_H_
#define SBITX_I2C_H_

#define ATTINY85_I2C 0x08
#define SI5351_I2C 0x60

#include <stdbool.h>
#include <stdint.h>

#include "sbitx_core.h"

bool i2c_open(radio *radio_h);
bool i2c_close(radio *radio_h);

int i2c_read_pwr_levels(radio *radio_h, uint8_t *response);
void i2c_write_si5351(radio *radio_h, uint8_t reg, uint8_t val);

#endif // SBITX_I2C_H_
