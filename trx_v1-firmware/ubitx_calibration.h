/*
    uBitx v6 HERMES firmware
    Copyright (C) 2022 Rhizomatica <rafael@rhizomatica.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    ubitx_calibration.h

*/

#ifndef __UBITX_CALIBRATION
#define __UBITX_CALIBRATION

#define CAL_FREQ 5000000
#define PLL_FREQ 875000000

#define PLL_FREQ_DIV_CAL_FREQ 175

#define PLL_FREQ_DIV_CAL_FREQ_DIV7 25

#define CAL_FREQ_TIMES7 35000000

extern bool gps_pps_tick;
extern bool calibration_enabled;
extern uint16_t tcount;
extern uint32_t mult;
extern uint32_t XtalFreq;


extern uint32_t calibration_monitor;

void enable_calibration();
void disable_calibration();

void PPSinterrupt();

#endif // __UBITX_CALIBRATION
