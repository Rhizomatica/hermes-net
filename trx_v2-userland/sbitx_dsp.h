/*
 * sBitx controller
 *
 * Copyright (C) 2023-2024 Rafael Diniz and Ashhar Farhan
 * Authors: Rafael Diniz <rafael@rhizomatica.org>
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

struct vfo {
	int freq_hz;
	int phase;
	int phase_increment;
};


// init and free, for initialization and shutdown procedures
void dsp_init(radio *radio_h);
void dsp_free(radio *radio_h);

// main DSP calls
void dsp_process_rx(uint8_t *buffer_radio_to_dsp, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t * output_tx, uint32_t block_size);
void dsp_process_tx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size, bool input_is_48k_stereo);

// zero fft previous bins
void fft_reset_m_bins();

// set the filters according to bpf_low and bpf_high of current profile
void dsp_set_filters();

// call the agc code
void dsp_process_agc();

// the the tx band multiplier
double get_band_multiplier();

// PTT-off hook used by tr_switch: if the active profile has digital
// voice enabled, ask the RADAE TX pipeline to emit its V2 end-of-over
// frame now.  Non-blocking; caller must delay any PA-drive drop long
// enough for the EOO IQ to flow through the DSP->DAC tail (empirically
// ~120-180 ms covers subprocess scheduling + ALSA buffering).
// Returns true iff the signal was sent; false if no active RADAE
// session or DV is off.
bool dsp_radae_tx_emit_eoo_if_dv(void);

// Companion to dsp_radae_tx_emit_eoo_if_dv: clear the RADAE TX
// flow-control state so the next PTT-on re-enters radae_tx_start
// cleanly.  Must be called only after the EOO IQ has had time to
// drain through DSP->DAC (same delay the caller already uses before
// dropping PA drive), otherwise radae_tx_stop will truncate the EOO
// frame.  No-op if RADAE TX was never active.
void dsp_radae_tx_end_over(void);

// by Ashhar Farhan, from https://github.com/afarhan/sbitx/blob/main/fft_filter.c
struct filter *filter_new(int input_length, int impulse_length);
int filter_tune(struct filter *f, double const low, double const high, double const kaiser_beta);
int window_filter(int const L,int const M,complex double * const response, double const beta);
int make_kaiser(double * const window,unsigned int const M, double const beta);
const double i0(double const z);

// https://github.com/afarhan/sbitx/blob/main/vfo.c
void vfo_init_phase_table();
void vfo_start(struct vfo *v, int frequency_hz, int start_phase);
int vfo_read(struct vfo *v);

void rational_resampler(double * in, int in_size, double * out, int rate, int interpolation_decimation);
double interpolate_linear(double  a,double a_x,double b,double b_x,double x);

#endif // SBITX_DSP_H_
