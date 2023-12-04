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

#include "sbitx_core.h"
#include "sbitx_i2c.h"
#include "sbitx_gpio.h"
#include "sbitx_si5351.h"

#include <string.h>
#include <wiringPi.h>
#include <unistd.h>

void hw_init(radio *radio_h)
{
    // I2C SETUP
    i2c_open(radio_h);

    // GPIO SETUP
    gpio_init(radio_h);

    // Si5351 SETUP
    setup_oscillators(radio_h);
}

void hw_shutdown(radio *radio_h)
{
    i2c_close(radio_h);

    // WiringPi has no function to close/shutdown resources
    // should we stop the Si5351 clocks?
}

// reads the power measurements from I2C bus
bool update_power_measurements(radio *radio_h)
{
	uint8_t response[4];
    uint16_t vfwd, vref;

    int count = i2c_read_pwr_levels(radio_h, response);

	if(count != 4)
		return false;

    memcpy(&vfwd, response, 2);
	memcpy(&vref, response+2, 2);

    radio_h->fwd_power = vfwd;
    radio_h->ref_power = vref;

    return true;
}

// returns power * 10
uint32_t get_fwd_power(radio *radio_h)
{
    // 40 should be we are using 40W as end of scale
	uint32_t fwdvoltage =  (radio_h->fwd_power * 40) / 100; // 100 = bridge_compensation
	uint32_t fwdpower = (fwdvoltage * fwdvoltage)/400;

    return fwdpower;
}

uint32_t get_ref_power(radio *radio_h)
{
	uint32_t refvoltage =  (radio_h->ref_power * 40) / radio_h->bridge_compensation;
	uint32_t refpower = (refvoltage * refvoltage) / 400;

    return refpower;

}

uint32_t get_swr(radio *radio_h)
{
    uint32_t vfwd = radio_h->fwd_power;
    uint32_t vref = radio_h->ref_power;
    uint32_t vswr;

    if (vref == vfwd)
        vfwd++;

    if (vref > vfwd)
		vswr = 100;
	else
		vswr = (10*(vfwd + vref))/(vfwd-vref);

    return vswr;
}

void set_frequency(radio *radio_h, uint32_t frequency)
{
    uint32_t *radio_freq = &radio_h->profiles[radio_h->profile_active_idx].frequency;

    *radio_freq = frequency;
     // Were we are setting the real frequency of the radio (in USB, which is the current setup), without
     // the 24 kHz offset as present in Ashhar implementation (just "- 24000" to replicate the behavior)
    si5351bx_setfreq(2, *radio_freq + radio_h->bfo_frequency);
}

void set_bfo(radio *radio_h, uint32_t frequency)
{
    radio_h->bfo_frequency = frequency;
    si5351bx_setfreq(1, radio_h->bfo_frequency);
}


void lpf_off(radio *radio_h)
{
    digitalWrite(LPF_A, LOW);
    digitalWrite(LPF_B, LOW);
    digitalWrite(LPF_C, LOW);
    digitalWrite(LPF_D, LOW);

}

void lpf_set(radio *radio_h)
{
    uint32_t *radio_freq = &radio_h->profiles[radio_h->profile_active_idx].frequency;

    int lpf = 0;

    if (*radio_freq < 5500000)
        lpf = LPF_D;
    else if (*radio_freq < 10500000)
        lpf = LPF_C;
    else if (*radio_freq < 18500000)
        lpf = LPF_B;
    else if (*radio_freq < 30000000)
        lpf = LPF_A;

    digitalWrite(lpf, HIGH);
}


void tr_switch(radio *radio_h, bool txrx_state){

    if (txrx_state == radio_h->txrx_state)
        return;

    if (txrx_state == IN_TX)
    {
        radio_h->txrx_state = IN_TX;

        lpf_off(radio_h); delay(2);
        digitalWrite(TX_LINE, HIGH); delay(2);
        lpf_set(radio_h);
    }
    else
    {
        radio_h->txrx_state = IN_RX;

        lpf_off(radio_h); delay(2);
        digitalWrite(TX_LINE, LOW); delay(2);
        lpf_set(radio_h);
    }
}
