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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>

#include "gpiolib/gpiolib.h"

#include "sbitx_gpio.h"

// global radio handle pointer used for the callback functions
static radio *radio_gpio_h;

extern _Atomic bool shutdown_;


// for now this initializes the GPIO and also initializes the structures for
// encoder/knobs for easy reading by application
void gpio_init(radio *radio_h)
{
    // thread is started after init, so no need for wrapped functions here (set_drive and get_level)
    pthread_mutex_init(&radio_h->gpio_mutex, NULL);

    // we need the radio handler available for the callbacks.
    radio_gpio_h = radio_h;

    // GPIOLIB initialization
    int ret = gpiolib_init();
    if (ret <= 0)
    {
        printf("Failed to initialise gpiolib.\n");
        shutdown_ = true;
        return ;
    }

    ret = gpiolib_mmap();
    if (ret)
    {
        if (ret == EACCES && geteuid())
            printf("Must be root\n");
        else
            printf("Failed to mmap gpiolib - %s\n", strerror(ret));
        shutdown_ = true;
        return ;
    }

    // INPUT pins
    unsigned int input_pins[8] = {ENC1_A, ENC1_B, ENC1_SW, ENC2_A, ENC2_B, ENC2_SW, PTT, DASH};
    for (int i = 0; i < 8; i++)
    {
        gpio_set_pull(input_pins[i], PULL_UP);
        gpio_set_fsel(input_pins[i], GPIO_FSEL_INPUT);
    }

    // OUTPUT pins
    // if on sBitx DE, add TX_POWER signal here and to tr_switch
    unsigned int output_pins[5] = {LPF_A, LPF_B, LPF_C, LPF_D, TX_LINE};
    for (int i = 0; i < 5; i++)
    {
        gpio_set_drive(output_pins[i], DRIVE_LOW);
        gpio_set_fsel(output_pins[i], GPIO_FSEL_OUTPUT);
    }

    // Initialize our two encoder structs (front pannel knobs)
    enc_init(&radio_h->enc_a, ENC_FAST, ENC1_B, ENC1_A);
    enc_init(&radio_h->enc_b, ENC_FAST, ENC2_A, ENC2_B);

    radio_h->volume_ticks = 0;
    radio_h->tuning_ticks = 0;

    // add our input pins to our poll()-like edge detection
    do_gpio_poll_add(ENC2_A);
    do_gpio_poll_add(ENC2_B);

    do_gpio_poll_add(ENC1_A);
    do_gpio_poll_add(ENC1_B);

    do_gpio_poll_add(ENC1_SW);
    do_gpio_poll_add(ENC2_SW);

    do_gpio_poll_add(PTT);
    do_gpio_poll_add(DASH);
}

// wrapped functions to ensure mutual exclusion to gpiolib calls
inline void set_drive(unsigned gpio, GPIO_DRIVE_T drv)
{
    pthread_mutex_lock(&radio_gpio_h->gpio_mutex);
    gpio_set_drive(gpio, drv);
    pthread_mutex_unlock(&radio_gpio_h->gpio_mutex);
}

inline int get_level(unsigned gpio)
{
    pthread_mutex_lock(&radio_gpio_h->gpio_mutex);
    int ret = gpio_get_level(gpio);
    pthread_mutex_unlock(&radio_gpio_h->gpio_mutex);

    return ret;
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
    if (get_level(PTT) == 0)
        radio_gpio_h->key_down = true;
    else
        radio_gpio_h->key_down = false;
}

void dash_change()
{
    if (get_level(DASH) == 0)
        radio_gpio_h->dash_down = true;
    else
        radio_gpio_h->dash_down = false;
}

void knob_a_pressed(void)
{
    static bool first = true;

    if (!first)
        radio_gpio_h->knob_a_pressed++;
    else
        first = false;
}

void knob_b_pressed(void)
{
    static bool first = true;

    if (!first)
        radio_gpio_h->knob_b_pressed++;
    else
        first = false;
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
    return (get_level(e->pin_a) ? 1 : 0) + (get_level(e->pin_b) ? 2: 0);
}

int enc_read(encoder *e)
{
    int result = 0;
    int newState;

    newState = enc_state(e); // Get current state

    if (newState != e->prev_state)
        usleep(1000);

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

// our poll infrastructure
struct poll_gpio_state {
    unsigned int num;
    unsigned int gpio;
    const char *name;
    int level;
};

static int num_poll_gpios;
static struct poll_gpio_state *poll_gpios;

int do_gpio_poll_add(unsigned int gpio)
{
    struct poll_gpio_state *new_gpio;
    unsigned int num = gpio;

    if (!gpio_num_is_valid(gpio))
        return 1;

    poll_gpios = reallocarray(poll_gpios, num_poll_gpios + 1,
                              sizeof(*poll_gpios));
    new_gpio = &poll_gpios[num_poll_gpios];
    new_gpio->num = num;
    new_gpio->gpio = gpio;
    new_gpio->name = gpio_get_name(gpio);
    new_gpio->level = -1; /* Unknown */
    num_poll_gpios++;

    return 0;
}

void *do_gpio_poll(void *radio_h_v)
{

    while (num_poll_gpios && !shutdown_)
    {
        int changed = 0;
        for (int i = 0; i < num_poll_gpios; i++)
        {
            struct poll_gpio_state *state = &poll_gpios[i];
            int level = get_level(state->gpio);
            if (level != state->level)
            {
                switch (state->gpio)
                {
                case PTT:
                    ptt_change();
                    break;
                case DASH:
                    dash_change();
                    break;
                case ENC1_A:
                    tuning_isr_a();
                    break;
                case ENC1_B:
                    tuning_isr_a();
                    break;
                case ENC1_SW:
                    knob_a_pressed();
                    break;
                case ENC2_A:
                    tuning_isr_b();
                    break;
                case ENC2_B:
                    tuning_isr_b();
                    break;
                case ENC2_SW:
                    knob_b_pressed();
                    break;
                default:
                    printf("Wrong GPIO\n");
                }
                state->level = level;
                changed = 1;
            }
        }
        if (!changed)
            usleep(1000);
    }

    return NULL;
}
