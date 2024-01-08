/* sBitx core
 * Copyright (C) 2023-2024 Rhizomatica
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

#include <stdbool.h>
#include <wiringPi.h>

#include "sbitx_gpio.h"

// global radio handle pointer used for the callback functions
radio *radio_gpio_h;

// for now this initializes the GPIO and also initializes the structures for
// encoder/knobs for easy reading by application
void gpio_init(radio *radio_h)
{
    // we need the radio handler available for the callbacks.
    radio_gpio_h = radio_h;

    // GPIO SETUP
    wiringPiSetup();

    // TODO: change the number to the #defines for easier reading
    char pins[13] = {0, 2, 3, 6, 7, 10, 11, 12, 13, 14, 21, 25, 27};
    for (int i = 0; i < 13; i++)
    {
        pinMode(pins[i], INPUT);
        pullUpDnControl(pins[i], PUD_UP);
    }

    //setup the LPFs and TX lines to initial state
    pinMode(TX_LINE, OUTPUT);
    pinMode(TX_POWER, OUTPUT);
    pinMode(LPF_A, OUTPUT);
    pinMode(LPF_B, OUTPUT);
    pinMode(LPF_C, OUTPUT);
    pinMode(LPF_D, OUTPUT);
    digitalWrite(LPF_A, LOW);
    digitalWrite(LPF_B, LOW);
    digitalWrite(LPF_C, LOW);
    digitalWrite(LPF_D, LOW);
    digitalWrite(TX_LINE, LOW);
    digitalWrite(TX_POWER, LOW);

    // Initialize our two encoder structs (front pannel knobs)
    enc_init(&radio_h->enc_a, ENC_FAST, ENC1_B, ENC1_A);
    enc_init(&radio_h->enc_b, ENC_FAST, ENC2_A, ENC2_B);

    radio_h->volume_ticks = 0;
    radio_h->tuning_ticks = 0;

    // Setting the callback for the encoders interrupts
    wiringPiISR(ENC2_A, INT_EDGE_BOTH, tuning_isr_b);
    wiringPiISR(ENC2_B, INT_EDGE_BOTH, tuning_isr_b);

    wiringPiISR(ENC1_A, INT_EDGE_BOTH, tuning_isr_a);
    wiringPiISR(ENC1_B, INT_EDGE_BOTH, tuning_isr_a);

    wiringPiISR(ENC1_SW, INT_EDGE_FALLING, knob_a_pressed);
    wiringPiISR(ENC2_SW, INT_EDGE_FALLING, knob_b_pressed);

    wiringPiISR(PTT, INT_EDGE_BOTH, ptt_change);
    wiringPiISR(DASH, INT_EDGE_BOTH, dash_change);
}


void enc_init(encoder *e, int speed, int pin_a, int pin_b)
{
    e->pin_a = pin_a;
    e->pin_b = pin_b;
    e->speed = speed;
    e->history = 5;
}

void ptt_change()
{
    if (digitalRead(PTT) == LOW)
        radio_gpio_h->key_down = true;
    else
        radio_gpio_h->key_down = false;
}

void dash_change()
{
    if (digitalRead(DASH) == LOW)
        radio_gpio_h->dash_down = true;
    else
        radio_gpio_h->dash_down = false;
}

void knob_a_pressed(void)
{
    radio_gpio_h->knob_a_pressed++;
}

void knob_b_pressed(void)
{
    radio_gpio_h->knob_b_pressed++;
}


void tuning_isr_a(void)
{
    static bool first = true;

    int tuning = enc_read(&radio_gpio_h->enc_a);

    if (!first)
    {
        if (tuning < 0)
            radio_gpio_h->volume_ticks++;
        if (tuning > 0)
            radio_gpio_h->volume_ticks--;
    }
    else
        first = false;
}

void tuning_isr_b(void)
{
    static bool first = true;

    int tuning = enc_read(&radio_gpio_h->enc_b);

    if (!first)
    {
        if (tuning < 0)
            radio_gpio_h->tuning_ticks++;
        if (tuning > 0)
            radio_gpio_h->tuning_ticks--;
    }
    else
        first = false;
}

int enc_state (encoder *e)
{
    return (digitalRead(e->pin_a) ? 1 : 0) + (digitalRead(e->pin_b) ? 2: 0);
}

int enc_read(encoder *e)
{
    int result = 0;
    int newState;

    newState = enc_state(e); // Get current state

    if (newState != e->prev_state)
        delay (1);

    if (enc_state(e) != newState || newState == e->prev_state)
        return 0;

    //these transitions point to the encoder being rotated anti-clockwise
    if ((e->prev_state == 0 && newState == 2) ||
        (e->prev_state == 2 && newState == 3) ||
        (e->prev_state == 3 && newState == 1) ||
        (e->prev_state == 1 && newState == 0))
    {
        e->history--;
    }
    //these transitions point to the enccoder being rotated clockwise
    if ((e->prev_state == 0 && newState == 1) ||
        (e->prev_state == 1 && newState == 3) ||
        (e->prev_state == 3 && newState == 2) ||
        (e->prev_state == 2 && newState == 0))
    {
        e->history++;
    }
    e->prev_state = newState; // Record state for next pulse interpretation
    if (e->history > e->speed)
    {
        result = 1;
        e->history = 0;
    }
    if (e->history < -e->speed)
    {
        result = -1;
        e->history = 0;
    }

    return result;
}
