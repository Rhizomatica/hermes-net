/*
 * sBitx controller
 *
 * Copyright (C) 2023-204 Rhizomatica
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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <complex.h>
#include <fftw3.h>
#include <math.h>

#include "sbitx_dsp.h"
#include "sbitx_core.h"

 // we read the mode sample format from here
extern snd_pcm_format_t format;

static radio *radio_h_dsp;

#define MAX_BINS 2048
#define TUNED_BINS 512

fftw_complex *fft_freq;
fftw_complex *fft_time;
fftw_complex *fft_out;		// holds the incoming samples in freq domain (for rx as well as tx)
fftw_complex *fft_in;			// holds the incoming samples in time domain (for rx as well as tx)
fftw_complex *fft_m;			// holds previous samples for overlap and discard convolution
fftw_plan plan_fwd, plan_tx, plan_rev;

struct filter *rx_filter;	// rx convolution filter
struct filter *tx_filter;	// tx convolution filter

// - signal_input: 96 kHz mono from radio, get the "slice" between 24 kHz and 27 kHz (USB) or 21 kHz to 24 kHz (LSB), and bring this slice to 0 and 3 kHz
// - out output_speaker: 96 kHz mono output for speaker
// - out output_loopback: 48 kHz stereo for loopback input
// - out output_tx: NULL buffer
// - block_size: number of samples
void dsp_process_rx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size)
{
	int i, j = 0;
	double i_sample, q_sample;

    int32_t *input_rx = (int32_t *) signal_input;

	//STEP 1: first add the previous M samples to
	memcpy(fft_in, fft_m, MAX_BINS/2 * sizeof(fftw_complex));

	//STEP 2: then add the new set of samples
	// m is the index into incoming samples, starting at zero
	// i is the index into the time samples, picking from
	// the samples added in the previous step
	int m = 0;
	//gather the samples into a time domain array
	for (i= MAX_BINS/2; i < MAX_BINS; i++, j++, m++){
		i_sample = (1.0  * input_rx[j]) / 200000000.0;
		q_sample = 0;

		__real__ fft_m[m] = i_sample;
		__imag__ fft_m[m] = q_sample;

		__real__ fft_in[i]  = i_sample;
		__imag__ fft_in[i]  = q_sample;
		m++;
	}

	// STEP 3: convert the time domain samples to  frequency domain
	fftw_execute(plan_fwd);

	//STEP 4: we rotate the bins around by r-tuned_bin
	for (i = 0; i < MAX_BINS; i++){
		int b =  i + TUNED_BINS;
		if (b >= MAX_BINS)
			b = b - MAX_BINS;
		if (b < 0)
			b = b + MAX_BINS;
		fft_freq[i] = fft_out[b];
	}

	// STEP 5:zero out the other sideband
	if (radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].mode == MODE_LSB)
        memset(fft_freq, 0, sizeof(fftw_complex) * (MAX_BINS/2));
	else
        memset((void *) fft_freq + (MAX_BINS/2 * sizeof(fftw_complex)), 0, sizeof(fftw_complex) * (MAX_BINS/2));

	// STEP 6: apply the filter to the signal,
	// in frequency domain we just multiply the filter
	// coefficients with the frequency domain samples
	for (i = 0; i < MAX_BINS; i++)
		fft_freq[i] *= rx_filter->fir_coeff[i];

	//STEP 7: convert back to time domain
	fftw_execute(plan_rev);

	//STEP 8 : AGC
    // TODO: re-add me!
	// agc2(r);

	//STEP 9: send the output back to where it needs to go
    int32_t *output_speaker_int = (int32_t *)output_speaker;
    for (i= 0; i < MAX_BINS / 2; i++)
    {
        int32_t sample = cimag(fft_time[i+(MAX_BINS/2)]);
        output_speaker_int[i] = sample;
    }

    // 96 kHz mono to 48 kHz stereo conversion
    int32_t *output_loopback_int = (int32_t *) output_loopback;
    for(i = 0; i < block_size; i += 2)
    {
        output_loopback_int[i] = output_speaker_int[i];
        output_loopback_int[i + 1] = output_speaker_int[i];
    }

    memset(output_tx, 0, block_size * (snd_pcm_format_width(format) / 8));
}

// - signal_input: 96 kHz mono input from mic or 48 kHz stereo input from loopback
// - output_speaker: NULL buffer
// - output_loopback: NULL buffer
// - output_tx: get baseband (0 to 3 kHz) and upconvert to LSB or USB as above
// - block_size: number of samples
// - input_is_48k_stereo: if true, input is 48 kHz stereo (loopback), otherwise, 96 kHz mono (mic)
void dsp_process_tx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size, bool input_is_48k_stereo)
{

    memset(output_loopback, 0, block_size * (snd_pcm_format_width(format) / 8));
    memset(output_speaker, 0, block_size * (snd_pcm_format_width(format) / 8));

}

void fft_reset_m_bins(){
	//zero up the previous 'M' bins
	memset(fft_in, 0, sizeof(fftw_complex) * MAX_BINS);
	memset(fft_out, 0, sizeof(fftw_complex) * MAX_BINS);
	memset(fft_m, 0, sizeof(fftw_complex) * MAX_BINS/2);
	memset(fft_time, 0, sizeof(fftw_complex) * MAX_BINS);
	memset(fft_freq, 0, sizeof(fftw_complex) * MAX_BINS);
}

void dsp_start(radio *radio_h)
{
    radio_h_dsp = radio_h;

    fft_m = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS/2);
	fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);
	fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);

	//create fft complex arrays to convert the frequency back to time
	fft_time = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);
	fft_freq = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);

    memset(fft_m, 0, sizeof(fftw_complex) * MAX_BINS/2);
	memset(fft_in, 0, sizeof(fftw_complex) * MAX_BINS);
	memset(fft_out, 0, sizeof(fftw_complex) * MAX_BINS);

    plan_rev = fftw_plan_dft_1d(MAX_BINS, fft_freq, fft_time, FFTW_BACKWARD, FFTW_ESTIMATE);
	plan_fwd = fftw_plan_dft_1d(MAX_BINS, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    // TODO: move this to a set_passband
    uint32_t bpf_low = radio_h->profiles[radio_h->profile_active_idx].bpf_low;
    uint32_t bpf_high = radio_h->profiles[radio_h->profile_active_idx].bpf_high;

    rx_filter = filter_new(1024, 1025);
	filter_tune(rx_filter, (1.0 * bpf_low)/96000.0, (1.0 * bpf_high)/96000.0 , 5);

	tx_filter = filter_new(1024, 1025);
	filter_tune(tx_filter, (1.0 * bpf_low)/96000.0, (1.0 * bpf_high)/96000.0 , 5);

}

struct filter *filter_new(int input_length, int impulse_length)
{
    struct filter *f = malloc(sizeof(struct filter));
    f->L = input_length;
    f->M = impulse_length;
    f->N = f->L + f->M - 1;
    f->fir_coeff = fftwf_alloc_complex(f->N);

    return f;
}

int filter_tune(struct filter *f, float const low,float const high,float const kaiser_beta)
{
    if(isnan(low) || isnan(high) || isnan(kaiser_beta))
        return -1;

    assert(fabs(low) <= 0.5);
    assert(fabs(high) <= 0.5);

    float gain = 1./((float)f->N);
    //printf("# Gain is %lf\n", gain);
	//printf("# filter elements %d\n", f->N);

    for(int n = 0; n < f->N; n++)
    {
        float s;
        //the first half is +ve frequencies in frequency domain
        if(n <= f->N/2)
            s = (float)n / f->N;
        else	//the second half is -ve frequencies, inverted
            s = (float)(n-f->N) / f->N;

        if(s >= low && s <= high)
            f->fir_coeff[n] = gain;
        else
            f->fir_coeff[n] = 0;
        // printf("#1 %d  %g  %g %g before windowing: %g,%g\n", n, s, low, high, creal(f->fir_coeff[n]), cimag(f->fir_coeff[n]));
  }


    window_filter(f->L, f->M, f->fir_coeff, kaiser_beta);
    return 0;
}

void filter_print(struct filter *f)
{
    printf("#Filter windowed FIR frequency coefficients\n");
    for(int n = 0; n < f->N; n++)
    {
        printf("%d,%.17f,%.17f\n", n, crealf(f->fir_coeff[n]), cimagf(f->fir_coeff[n]));
    }
}

int window_filter(int const L,int const M,complex float * const response,float const beta)
{

    //total length of the convolving samples
    int const N = L + M - 1;

    // fftw_plan can overwrite its buffers, so we're forced to make a temp. Ugh.
    complex float * const buffer = fftwf_alloc_complex(N);
    fftwf_plan fwd_filter_plan = fftwf_plan_dft_1d(N,buffer,buffer,FFTW_FORWARD,FFTW_ESTIMATE);
    fftwf_plan rev_filter_plan = fftwf_plan_dft_1d(N,buffer,buffer,FFTW_BACKWARD,FFTW_ESTIMATE);

    // Convert to time domain
    memcpy(buffer, response, N * sizeof(*buffer));
    fftwf_execute(rev_filter_plan);
    fftwf_destroy_plan(rev_filter_plan);

    float kaiser_window[M];
    make_kaiser(kaiser_window,M,beta);

    // Round trip through FFT/IFFT scales by N
    float const gain = 1.;

    //shift the buffer to make it causal
    for(int n = M - 1; n >= 0; n--)
        buffer[n] = buffer[ (n-M/2+N) % N];

    // apply window and gain
    for(int n = M - 1; n >= 0; n--)
        buffer[n] = buffer[n] * kaiser_window[n] * gain;

    // Pad with zeroes on right side
    memset(buffer+M,0,(N-M)*sizeof(*buffer));

    // Now back to frequency domain
    fftwf_execute(fwd_filter_plan);
    fftwf_destroy_plan(fwd_filter_plan);

    memcpy(response,buffer,N*sizeof(*response));

    fftwf_free(buffer);
    return 0;
}

// Compute an entire Kaiser window
// More efficient than repeatedly calling kaiser(n,M,beta)
int make_kaiser(float * const window,unsigned int const M,float const beta)
{
    assert(window != NULL);
    if(window == NULL)
        return -1;
    // Precompute unchanging partial values
    float const numc = M_PI * beta;
    float const inv_denom = 1. / i0(numc); // Inverse of denominator
    float const pc = 2.0 / (M-1);

    // The window is symmetrical, so compute only half of it and mirror
    // this won't compute the middle value in an odd-length sequence
    for(int n = 0; n < M/2; n++)
    {
        float const p = pc * n  - 1;
        window[M-1-n] = window[n] = i0(numc * sqrtf(1-p*p)) * inv_denom;
    }
    // If sequence length is odd, middle value is unity
    if(M & 1)
        window[(M-1)/2] = 1; // The -1 is actually unnecessary

    return 0;
}

// Modified Bessel function of the 0th kind, used by the Kaiser window
const float i0(float const z)
{
    const float t = (z*z)/4;
    float sum = 1 + t;
    float term = t;
    for(int k=2; k<40; k++){
        term *= t/(k*k);
        sum += term;
        if(term < 1e-12 * sum)
            break;
    }
    return sum;
}
