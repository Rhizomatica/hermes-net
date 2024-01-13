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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "sbitx_core.h"
#include "sbitx_i2c.h"
#include "sbitx_gpio.h"
#include "sbitx_si5351.h"

#include "gpiolib/gpiolib.h"

extern _Atomic bool shutdown;

// this is our main 10ms period io loop
void io_tick(radio *radio_h)
{
    static uint64_t ticks = 0;
    _Atomic uint32_t freq = radio_h->profiles[radio_h->profile_active_idx].freq;
    _Atomic uint32_t volume = radio_h->profiles[radio_h->profile_active_idx].speaker_level;
    _Atomic uint32_t tuning_step = radio_h->profiles[radio_h->profile_active_idx].step_size;

    bool set_dirty = false;

    ticks++;

    // a speed up if one tunes the knob fast
    if (radio_h->profiles[radio_h->profile_active_idx].enable_knob_frequency && radio_h->tuning_ticks)
    {
        if (abs(radio_h->tuning_ticks) > 50)
            radio_h->tuning_ticks *= 4;

        while (radio_h->tuning_ticks > 0)
        {
            radio_h->tuning_ticks--;
            freq -= tuning_step;
        }
        while (radio_h->tuning_ticks < 0)
        {
            radio_h->tuning_ticks++;
            freq += tuning_step;
        }
        set_dirty = true;
        set_frequency(radio_h, freq);
    }

    if (radio_h->profiles[radio_h->profile_active_idx].enable_knob_volume && radio_h->volume_ticks)
    {
        if (abs(radio_h->volume_ticks) > 50)
            radio_h->volume_ticks *= 2;

        while (radio_h->volume_ticks > 0)
        {
            radio_h->volume_ticks--;
            if (volume < 5)
                volume = 0;
            else
                volume -= 4;
        }
        while (radio_h->volume_ticks < 0)
        {
            radio_h->volume_ticks++;
            if (volume > 95)
                volume = 100;
            else
                volume += 4;
        }
        set_dirty = true;
        // TODO: put everything on a set_speaker_level()
        // radio_h->profiles[radio_h->profile_active_idx].speaker_level = volume;
        // set_speaker_level(radio_h, volume);
	}

    // period * 3, read power over i2c
	if ( !(ticks % 3) && radio_h->txrx_state == IN_TX )
    {
            update_power_measurements(radio_h);
    }

#if 0 // we are not using the button presses for nothing up to now
	if (!(ticks % 10))
    {
		if (radio_h->knob_a_pressed)
        {
            printf("Button A pressed\n");
            radio_h->knob_a_pressed = 0;
        }
		if (radio_h->knob_b_pressed)
        {
            printf("Button B pressed\n");
            radio_h->knob_b_pressed = 0;
        }
    }
#endif

    if (set_dirty)
    {
        radio_h->cfg_user_dirty = true;
        // TODO: WebSockets...
        //send_ws_update = true;
    }

}



bool hw_init(radio *radio_h, pthread_t *hw_tid)
{
    // I2C SETUP
    i2c_open(radio_h);

    // GPIO SETUP
    gpio_init(radio_h);

    // Si5351 SETUP
    setup_oscillators(radio_h);

    // start hw io monitor thread, ref/pwr readings, volume and freq changes
    pthread_create(hw_tid, NULL, hw_thread, (void *) radio_h);

    return true;
}


bool hw_shutdown(radio *radio_h, pthread_t *hw_tid)
{
    i2c_close(radio_h);

    // WiringPi has no function to close/shutdown resources
    // should we stop the Si5351 clocks?

    pthread_join(*hw_tid, NULL);

    return true;
}

void *hw_thread(void *radio_h_v)
{
    uint64_t counter = 0;
    radio *radio_h = (radio *) radio_h_v;

    // starts our 1ms timer
    int res = start_periodic_timer(1000);

    if (res < 0)
    {
        printf("Fatal error: Start Periodic Timer\n");
        shutdown = true;
        return false;
    }

    while(!shutdown)
    {
        wait_next_activation();
        do_gpio_poll();
        if (!(counter % 10)) // we want 10ms io_tick()
            io_tick(radio_h);
        counter++;
    }

    return NULL;
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
    uint32_t *radio_freq = &radio_h->profiles[radio_h->profile_active_idx].freq;

    *radio_freq = frequency;
     // Were we are not setting the real frequency of the radio (in USB, which is the current setup)
     // We add 24 kHz offset in order to use Ashhar's DSP code (just "- 24000")
    si5351bx_setfreq(2, *radio_freq + radio_h->bfo_frequency - 24000);

    // TODO: put this inside a function in cfg_utils
    char tmp1[64]; char tmp2[64];
    sprintf(tmp1, "profile%u:freq", radio_h->profile_active_idx);
    sprintf(tmp2, "%u", *radio_freq);
    int rc = iniparser_set(radio_h->cfg_user, tmp1, tmp2);
    if (rc != 0)
        printf("Error modifying config file\n");

    radio_h->cfg_user_dirty = true;
}

void set_bfo(radio *radio_h, uint32_t frequency)
{
    radio_h->bfo_frequency = frequency;
    si5351bx_setfreq(1, radio_h->bfo_frequency);

    // TODO: put this inside a function in cfg_utils
    char tmp[64];
    sprintf(tmp, "%u", radio_h->bfo_frequency);
    int rc = iniparser_set(radio_h->cfg_core, "main:bfo", tmp);
    if (rc != 0)
        printf("Error modifying config file\n");

    radio_h->cfg_core_dirty = true;
}


void lpf_off(radio *radio_h)
{
    gpio_set_drive(LPF_A, DRIVE_LOW);
    gpio_set_drive(LPF_B, DRIVE_LOW);
    gpio_set_drive(LPF_C, DRIVE_LOW);
    gpio_set_drive(LPF_D, DRIVE_LOW);
}

void lpf_set(radio *radio_h)
{
    uint32_t *radio_freq = &radio_h->profiles[radio_h->profile_active_idx].freq;

    int lpf = 0;

    if (*radio_freq < 5250000)
        lpf = LPF_D;
    else if (*radio_freq < 10500000)
        lpf = LPF_C;
    else if (*radio_freq < 18500000)
        lpf = LPF_B;
    else if (*radio_freq < 35000000)
        lpf = LPF_A;

    gpio_set_drive(lpf, DRIVE_HIGH);
}


void tr_switch(radio *radio_h, bool txrx_state)
{
    if (txrx_state == radio_h->txrx_state)
        return;

    if (txrx_state == IN_TX)
    {
        radio_h->txrx_state = IN_TX;
        lpf_off(radio_h);
        usleep(2000);
        gpio_set_drive(TX_LINE, DRIVE_HIGH);
        usleep(2000);
        lpf_set(radio_h);
    }
    else
    {
        radio_h->txrx_state = IN_RX;
        lpf_off(radio_h);
        usleep(2000);
        gpio_set_drive(TX_LINE, DRIVE_LOW);
        usleep(2000);
        lpf_set(radio_h);
    }
}


// auxiliary functions for timer functionality
static struct timespec r;
static uint64_t period;
#define NSEC_PER_SEC 1000000000ULL

static inline void timespec_add_us(struct timespec *t, uint64_t d)
{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
}

void wait_next_activation(void)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &r, NULL);
    timespec_add_us(&r, period);
}

int start_periodic_timer(uint64_t offset)
{
    clock_gettime(CLOCK_REALTIME, &r);
    timespec_add_us(&r, offset);
    period = offset;

    return 0;
}
