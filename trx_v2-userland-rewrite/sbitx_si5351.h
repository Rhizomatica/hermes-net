/* sBitx core
 * Copyright (C) 2017-2023 Jerry Gaffke, KE7ER
 *
 * GPLv3 code from Jerry Gaffe, "An minimalist standalone set of Si5351
 * routines", later modified by Ashhar Farhan and Rafael Diniz.
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

#ifndef SBITX_SI5351_H_
#define SBITX_SI5351_H_

#include <stdint.h>
#include "sbitx_core.h"

void setup_oscillators(radio *radio_h);

void si5351_set_calibration(int32_t cal);
void si5351bx_init(); 
void si5351bx_setfreq(uint8_t clknum, uint32_t fout);
void si5351_reset();
void si5351a_clkoff(uint8_t clk);

#endif
