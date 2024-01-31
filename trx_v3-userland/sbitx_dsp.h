/*
 * sBitx controller
 *
 * Copyright (C) 2023-2024 Rafael Diniz, Ashhar Farhan and Fadi Jerji
 * Authors: Rafael Diniz <rafael@rhizomatica.org>
 *          Fadi Jerji <fadi.jerji@rhizomatica.org>
 *          Ashhar Farhan <afarhan@gmail.com>
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

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>

#include "sbitx_core.h"

#define INTERPOLATION 0
#define DECIMATION 1

struct filter
{
    complex double *fir_coeff;
    complex double *overlap;
    int N;
    int L;
    int M;
};

// init and free, for initialization and shutdown procedures
void dsp_init(radio *radio_h);
void dsp_free(radio *radio_h);

// main DSP calls
void dsp_process_rx(uint8_t *buffer_radio_to_dsp, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t * output_tx, uint32_t block_size);
void dsp_process_tx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size, bool input_is_48k_stereo);

// set the filters according to bpf_low and bpf_high of current profile
void dsp_set_filters();

// by Ashhar Farhan, from https://github.com/afarhan/sbitx/blob/main/fft_filter.c
struct filter *filter_new(int input_length, int impulse_length);
int filter_tune(struct filter *f, double const low, double const high, double const kaiser_beta);
int window_filter(int const L,int const M,complex double * const response, double const beta);
int make_kaiser(double * const window,unsigned int const M, double const beta);
const double i0(double const z);

// by Fadi Jerji @ Rhizomatica
void rational_resampler(double * in, int in_size, double * out, int rate, int interpolation_decimation);
double interpolate_linear(double  a,double a_x,double b,double b_x,double x);

#endif // SBITX_DSP_H_
