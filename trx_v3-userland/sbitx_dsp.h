/*
 * sBitx controller
 *
 * Copyright (C) 2023-2024 Rhizomatica
 * Author: Rafael Diniz <rafael@rhizomatica.org>
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


#ifndef SBITX_DSP_H_
#define SBITX_DSP_H_

#include <complex.h>

#include "sbitx_core.h"

struct filter
{
    complex float *fir_coeff;
    complex float *overlap;
    int N;
    int L;
    int M;
};

#include <stdint.h>
#include <stdbool.h>

void dsp_process_rx(uint8_t *buffer_radio_to_dsp, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t * output_tx, uint32_t block_size);

void dsp_process_tx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size, bool input_is_48k_stereo);

void dsp_start(radio *radio_h);

// DSP, from https://github.com/afarhan/sbitx/blob/main/fft_filter.c
struct filter *filter_new(int input_length, int impulse_length);
int filter_tune(struct filter *f, float const low,float const high,float const kaiser_beta);
int window_filter(int const L,int const M,complex float * const response,float const beta);
int make_kaiser(float * const window,unsigned int const M,float const beta);
const float i0(float const z);


#endif // SBITX_DSP_H_
