/* sBitx HERMES controller
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


#ifndef SBITX_GPIO_H_
#define SBITX_GPIO_H_

#include "sbitx_core.h"

#include "gpiolib/gpiolib.h"

void gpio_init(radio *radio_h);

void set_drive(unsigned gpio, GPIO_DRIVE_T drv);
int get_level(unsigned gpio);

// callback functions
void tuning_isr_a(void);
void tuning_isr_b(void);
void knob_a_pressed(void);
void knob_b_pressed(void);
void ptt_change(void);
void dash_change(void);

// encoder-related functions
void enc_init(encoder *e, int speed, int pin_a, int pin_b);
int enc_state (encoder *e);
int enc_read(encoder *e);

int do_gpio_poll_add(unsigned int gpio);
void *do_gpio_poll(void *radio_h_v);

#endif // SBITX_GPIO_H_
