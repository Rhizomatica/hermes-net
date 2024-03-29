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

    ubitx_calibration.cpp

*/

#if RADUINO_VER > 0

#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "ubitx.h"
#include "ubitx_calibration.h"

bool gps_pps_tick = false;
bool calibration_enabled = false;

uint16_t tcount = 0;
uint32_t mult = 0;

uint32_t XtalFreq = 0;

uint32_t calibration_monitor = 0;

void enable_calibration()
{
//    gps_operation_result = GPS_STATUS_PPS_FAIL | GPS_STATUS_OFFSET_BAD;

    calibration_monitor = millis();

    // we avoid overflow
    if ( (calibration_monitor + 12500) < calibration_monitor)
    {
        return;
    }

    calibration_enabled = true;

    attachInterrupt(digitalPinToInterrupt(PPS_IN), PPSinterrupt, RISING);

#if RADUINO_VER == 1
    si5351bx_setfreq(0, CAL_FREQ); // we change the CLK #0 to the calibration clock
#endif

#if RADUINO_VER == 2
    si5351bx_setfreq(3, CAL_FREQ); // we change the CLK #3 to the calibration clock
#endif
}

void disable_calibration()
{
// no need as this is called by si5351_set_calibration() from setMasterCal
//    si5351bx_setfreq(0, usbCarrier);
    detachInterrupt(digitalPinToInterrupt(PPS_IN));

    calibration_enabled = false;
    calibration_monitor = 0;

}



void PPSinterrupt()
{
    tcount++;

    gps_pps_tick = true;                        // New second by GPS.

    if (tcount == 3)                               // Start counting the xxx MHz signal from Si5351A CLK0
    {

        TCNT1 = 0;                                   //Reset count to zero
        mult = 0;
        TCCR1B = 7;                                  //Clock on rising edge of pin 5
    }
    else if (tcount == 10)                         // 7s of counting
    {
        TCCR1B = 0;                                  //Turn off counter
        XtalFreq = (mult << 16) + TCNT1;          //Calculate correction factor, eg. 5Mhz: multi * 65536 + ... =(deve ser)4= 5 MHz * 40 (s)
        TCNT1 = 0;                                   //Reset count to zero
        mult = 0;
        tcount = 0;                                  //Reset the seconds counter
    }

}

// Timer 1 overflow intrrupt vector.
ISR(TIMER1_OVF_vect)
{
    mult++;                                          //Increment multiplier
    TIFR1 = (1<<TOV1);                               //Clear overlow flag 
}

#endif
