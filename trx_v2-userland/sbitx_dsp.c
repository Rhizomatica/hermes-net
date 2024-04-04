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
 * Based on https://github.com/afarhan/sbitx/blob/main/sbitx.c and Jerji's
 * code
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <complex.h>
#include <fftw3.h>
#include <math.h>


#define USE_FFTW
#define LIBCSDR_GPL
#include <fft_fftw.h>
#include <libcsdr.h>
#include <libcsdr_gpl.h>


#include "sbitx_dsp.h"
#include "sbitx_core.h"
#include "sbitx_alsa.h"

// set 0 for production
#ifndef DEBUG_DSP_
#define DEBUG_DSP_ 0
#endif

 // we read the mode sample format from here
extern snd_pcm_format_t format;

extern _Atomic bool shutdown_;

static radio *radio_h_dsp;


#define MAX_BINS 2048
#define TUNED_BINS 512

#define MAX_SAMPLE_VALUE 8388607.0 // (2 ^ 23) - 1,  signed(?) 24 bits packed in a SIGNED 32 LE


fftw_complex *fft_freq;
fftw_complex *fft_time;
fftw_complex *fft_out;		// holds the incoming samples in freq domain (for rx as well as tx)
fftw_complex *fft_in;			// holds the incoming samples in time domain (for rx as well as tx)
fftw_complex *fft_m;			// holds previous samples for overlap and discard convolution
fftw_complex *buffer_filter;  // holds filter fft
fftw_plan plan_fwd, plan_tx, plan_rev;
fftw_plan fwd_filter_plan, rev_filter_plan;

struct filter *rx_filter;	// rx convolution filter
struct filter *tx_filter;	// tx convolution filter

struct vfo tone; // vfo

_Atomic bool tx_starting = false;
_Atomic bool rx_starting = false;

// - signal_input: 96 kHz mono from radio, get the "slice" between 24 kHz and 27 kHz (USB) or 21 kHz to 24 kHz (LSB), and bring this slice to 0 and 3 kHz
// - out output_speaker: 96 kHz mono output for speaker
// - out output_loopback: 48 kHz stereo for loopback input
// - out output_tx: NULL buffer
// - block_size: number of samples
void dsp_process_rx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size)
{
    double i_sample;

    int32_t *input_rx = (int32_t *) signal_input;

    //fix the burst at the start of transmission
    if (rx_starting)
    {
        fft_reset_m_bins();
        clear_buffers();
        rx_starting = false;
    }

    //STEP 1: first add the previous M samples to
    memcpy(fft_in, fft_m, MAX_BINS/2 * sizeof(fftw_complex));

    //STEP 2: then add the new set of samples
    // m is the index into incoming samples, starting at zero
    // i is the index into the time samples, picking from
    // the samples added in the previous step
    int i, j = 0;

#if DEBUG_DSP_ == 1
    static double max_i_sample = -100000000;
    static double min_i_sample = 100000000;
#endif
    //gather the samples into a time domain array
    for (i = MAX_BINS / 2; i < MAX_BINS; i++, j++)
    {
        // 24 bit audio samples are packed in MSB in the 32 bit word
        i_sample = (1.0  * (input_rx[j] >> 8)) / MAX_SAMPLE_VALUE;

#if DEBUG_DSP_ == 1
        if (max_i_sample < i_sample)
        {
            max_i_sample = i_sample;
            printf("input_rx %d\n", input_rx[j]);
            printf("max_i_sample %f\n\n", max_i_sample);
        }
        if (min_i_sample > i_sample)
        {
            min_i_sample = i_sample;
            printf("input_rx %d\n", input_rx[j]);
            printf("min_i_sample %f\n\n", min_i_sample);
        }
#endif

        __real__ fft_m[j] = i_sample;
        __imag__ fft_m[j] = 0;

        __real__ fft_in[i]  = i_sample;
        __imag__ fft_in[i]  = 0;
    }

	// STEP 3: convert the time domain samples to  frequency domain
	fftw_execute(plan_fwd);

	//STEP 4: we rotate the bins around by r-tuned_bin
    for (i = 0; i < MAX_BINS; i++)
    {
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
    if (radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].agc != AGC_OFF)
        dsp_process_agc();

	//STEP 9: send the output back to where it needs to go
    int32_t *output_speaker_int = (int32_t *)output_speaker;
    int32_t *output_loopback_int = (int32_t *) output_loopback;
    for (i = 0; i < block_size; i++)
    {
        output_speaker_int[i] = (int32_t) (cimag(fft_time[i+(MAX_BINS/2)]) * MAX_SAMPLE_VALUE);
        if ((i % 2) == 0)
        {
            output_loopback_int[i] = output_speaker_int[i] << 4; // we give a small gain here for the loopback
            output_loopback_int[i + 1] = output_loopback_int[i];     // 96 kHz mono to 48 kHz stereo decimation, L=R        }
        }
        // we shift 8 bit right to revert to wm8731 32-bit sample format (only 24-bit MSB valid)
        output_speaker_int[i] <<= 8;
    }

    memset(output_tx, 0, block_size * (snd_pcm_format_width(format) / 8));
}

// - signal_input: 96 kHz mono input from mic or 48 kHz stereo input from loopback
// - output_speaker: NULL buffer
// - output_loopback: NULL buffer
// - output_tx: output to tx with the input baseband (eg. 0 to 3 kHz) upconverted to passband in LSB or USB
// - block_size: number of samples, hardcoded to 1024 for now
// - input_is_48k_stereo: if true, input is 48 kHz stereo (loopback), otherwise, 96 kHz mono (mic)
void dsp_process_tx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size, bool input_is_48k_stereo)
{
    static double loopback_in[512]; // n_samples / 2
    static double signal_input_f[1024]; // n_samples

    int32_t *signal_input_int = (int32_t *) signal_input;
    int32_t *signal_output_int = (int32_t *) output_tx;

    if (block_size != 1024)
    {
        fprintf(stderr, "This code uses 1024 samples for the fft\n");
        shutdown_ = true;
        return;
    }

	//fix the burst at the start of transmission
    if (tx_starting)
    {
        fft_reset_m_bins();
        clear_buffers();
        tx_starting = false;
    }

	//first add the previous M samples
	memcpy(fft_in, fft_m, MAX_BINS/2 * sizeof(fftw_complex));

    double i_sample;
    int i, j = 0;
    // prepare data from loopback... 48kHz stereo to 96 kHz mono
    if (radio_h_dsp->tone_generation)
    {
        for (i = 0; i < block_size; i++)
        {
            signal_input_f[i] = (1.0 * vfo_read(&tone)) / 4800000000.0;
        }
    }
    else if (input_is_48k_stereo)
    {
        for (i = 0; i < block_size; i = i + 2)
        {
            // just left channel
            loopback_in[i/2] = (1.0 * (signal_input_int[i] >> 8)) / MAX_SAMPLE_VALUE;
        }
        rational_resampler(loopback_in, block_size / 2, signal_input_f, 2, INTERPOLATION);
    }
    else // mic input from wm8731
    {
        for (i = 0; i < block_size; i++)
        {
            signal_input_f[i] = (1.0 * (signal_input_int[i] >> 8)) / MAX_SAMPLE_VALUE;
        }
    }

#if DEBUG_DSP_ == 1
    static double max_i_sample = -100000000;
    static double min_i_sample = 100000000;
#endif

	//gather the samples into a time domain array
	for (i = MAX_BINS/2; i < MAX_BINS; i++, j++)
    {
        i_sample = signal_input_f[j];

#if DEBUG_DSP_ == 1
        if (max_i_sample < i_sample)
        {
            max_i_sample = i_sample;
            printf("input_tx %d\n", signal_input_int[j]);
            printf("max_tx_sample %f\n\n", max_i_sample);
        }
        if (min_i_sample > i_sample)
        {
            min_i_sample = i_sample;
            printf("input_tx %d\n", signal_input_int[j]);
            printf("min_tx_sample %f\n\n", min_i_sample);
        }
#endif

        __real__ fft_m[j] = i_sample;
        __imag__ fft_m[j] = 0;

        __real__ fft_in[i]  = i_sample;
        __imag__ fft_in[i]  = 0;
	}

	//convert to frequency
	fftw_execute(plan_fwd);

	// NOTE: fft_out holds the fft output (in freq domain) of the
	// incoming mic samples
	// the naming is unfortunate

	// apply the filter
    for (i = 0; i < MAX_BINS; i++)
		fft_out[i] *= tx_filter->fir_coeff[i];

	// the usb extends from 0 to MAX_BINS/2 - 1,
	// the lsb extends from MAX_BINS - 1 to MAX_BINS/2 (reverse direction)
	// zero out the other sideband

	// TBD: Something strange is going on, this should have been the otherway
	if (radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].mode == MODE_LSB)
        memset(fft_out, 0, sizeof(fftw_complex) * (MAX_BINS/2));
	else
        memset((void *) fft_out + (MAX_BINS/2 * sizeof(fftw_complex)), 0, sizeof(fftw_complex) * (MAX_BINS/2));

	//now rotate to the tx_bin
    for (i = 0; i < MAX_BINS; i++)
    {
        int b = i + TUNED_BINS;
        if (b >= MAX_BINS)
            b = b - MAX_BINS;
        if (b < 0)
            b = b + MAX_BINS;
        fft_freq[b] = fft_out[i];
    }

    //convert back to time domain
    fftw_execute(plan_rev);

    // TODO: Add the tx calibration gain here!!
    double multiplier = get_band_multiplier();
    for (i = 0; i < MAX_BINS / 2; i++)
    {
        signal_output_int[i] = (int32_t) (creal(fft_time[i+(MAX_BINS/2)]) * 4000.0 * multiplier); // we just chose an appropriate level...
        signal_output_int[i] <<= 8;
    }

    memset(output_loopback, 0, block_size * (snd_pcm_format_width(format) / 8));
    memset(output_speaker, 0, block_size * (snd_pcm_format_width(format) / 8));
}

double get_band_multiplier()
{
    static uint32_t freq_last = 0;
    static double multiplier = 0;

    if (freq_last != radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].freq)
    {
        freq_last = radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].freq;
        multiplier = 0;

        for (int k = 0; k < radio_h_dsp->band_power_count; k++)
        {
            if (freq_last >= radio_h_dsp->band_power[k].f_start && freq_last < radio_h_dsp->band_power[k].f_stop)
            {
                multiplier = radio_h_dsp->band_power[k].scale;
                break;
            }
        }

        if (multiplier == 0)
        {
            fprintf(stderr, "Error finding band gain!\n");
        }
    }

    return multiplier;
}

//zero up the previous 'M' bins
void fft_reset_m_bins()
{

    memset(fft_in, 0, sizeof(fftw_complex) * MAX_BINS);
    memset(fft_out, 0, sizeof(fftw_complex) * MAX_BINS);
    memset(fft_m, 0, sizeof(fftw_complex) * MAX_BINS/2);
    memset(fft_time, 0, sizeof(fftw_complex) * MAX_BINS);
    memset(fft_freq, 0, sizeof(fftw_complex) * MAX_BINS);
}

void dsp_init(radio *radio_h)
{
    radio_h_dsp = radio_h;

    if (radio_h->profiles[radio_h->profile_active_idx].operating_mode == OPERATING_MODE_CONTROLS_ONLY)
        return;

    fft_m = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS/2);
    fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);

    //create fft complex arrays to convert the frequency back to time
    fft_time = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);
    fft_freq = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * MAX_BINS);

    fft_reset_m_bins();

    printf("Creating signal FFT plans\n");
    plan_rev = fftw_plan_dft_1d(MAX_BINS, fft_freq, fft_time, FFTW_BACKWARD, FFTW_MEASURE);
    plan_fwd = fftw_plan_dft_1d(MAX_BINS, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);

    fft_reset_m_bins();

    rx_filter = filter_new(1024, 1025);
    tx_filter = filter_new(1024, 1025);

    printf("Creating filters FFT plans\n");
    dsp_set_filters();

    // init vfo for tone generation
    vfo_init_phase_table();
    vfo_start(&tone, 1000, 0);
}

void dsp_free(radio *radio_h)
{
    if (radio_h->profiles[radio_h->profile_active_idx].operating_mode == OPERATING_MODE_CONTROLS_ONLY)
        return;

    fftw_destroy_plan(plan_rev);
    fftw_destroy_plan(plan_fwd);
    fftw_destroy_plan(rev_filter_plan);
    fftw_destroy_plan(fwd_filter_plan);
    fftw_free(fft_m);
    fftw_free(fft_in);
    fftw_free(fft_out);
    fftw_free(fft_time);
    fftw_free(fft_freq);
    fftw_free(buffer_filter);
    free(rx_filter);
    free(tx_filter);
}

void dsp_set_filters()
{
    if (radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].operating_mode == OPERATING_MODE_CONTROLS_ONLY)
        return;

    double bpf_low = (double) radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].bpf_low;
    double bpf_high = (double) radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].bpf_high;

    if(radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].mode == MODE_LSB)
    {
        filter_tune(tx_filter, (1.0 * -bpf_high) / 96000.0, (1.0 * -bpf_low) / 96000.0, 5);
        filter_tune(rx_filter, (1.0 * -bpf_high) / 96000.0, (1.0 * -bpf_low) / 96000.0, 5);
    }
    else
    {
        filter_tune(tx_filter, (1.0 * bpf_low) / 96000.0, (1.0 * bpf_high) / 96000.0, 5);
        filter_tune(rx_filter, (1.0 * bpf_low) / 96000.0, (1.0 * bpf_high) / 96000.0, 5);
    }
}


struct filter *filter_new(int input_length, int impulse_length)
{
    struct filter *f = malloc(sizeof(struct filter));
    f->L = input_length;
    f->M = impulse_length;
    f->N = f->L + f->M - 1;
    f->fir_coeff = fftw_alloc_complex(f->N);

    return f;
}

int filter_tune(struct filter *f, double const low, double const high, double const kaiser_beta)
{
    if(isnan(low) || isnan(high) || isnan(kaiser_beta))
        return -1;

    assert(fabs(low) <= 0.5);
    assert(fabs(high) <= 0.5);

    double gain = 1./((float)f->N);
    //printf("# Gain is %lf\n", gain);
	//printf("# filter elements %d\n", f->N);

    for(int n = 0; n < f->N; n++)
    {
        double s;
        //the first half is +ve frequencies in frequency domain
        if(n <= f->N/2)
            s = (double) n / f->N;
        else	//the second half is -ve frequencies, inverted
            s = (double) (n-f->N) / f->N;

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

int window_filter(int const L,int const M,complex double * const response,double const beta)
{
    static int last_N = 0;

    //total length of the convolving samples
    int const N = L + M - 1;

    if (last_N != N)
    {
        if (last_N != 0)
        {
            printf("Re-creating filters FFT plans\n");
            fftw_destroy_plan(rev_filter_plan);
            fftw_destroy_plan(fwd_filter_plan);
            fftw_free(buffer_filter);
        }
        buffer_filter = fftw_alloc_complex(N);
        fwd_filter_plan = fftw_plan_dft_1d(N, buffer_filter, buffer_filter, FFTW_FORWARD, FFTW_MEASURE);
        rev_filter_plan = fftw_plan_dft_1d(N, buffer_filter, buffer_filter, FFTW_BACKWARD, FFTW_MEASURE);
        last_N = N;
    }

    // Convert to time domain
    memcpy(buffer_filter, response, N * sizeof(*buffer_filter));
    fftw_execute(rev_filter_plan);

    double kaiser_window[M];
    make_kaiser(kaiser_window,M,beta);

    // Round trip through FFT/IFFT scales by N
    double const gain = 1.;

    //shift the buffer to make it causal
    for(int n = M - 1; n >= 0; n--)
        buffer_filter[n] = buffer_filter[ (n-M/2+N) % N];

    // apply window and gain
    for(int n = M - 1; n >= 0; n--)
        buffer_filter[n] = buffer_filter[n] * kaiser_window[n] * gain;

    // Pad with zeroes on right side
    memset(buffer_filter+M,0,(N-M)*sizeof(*buffer_filter));

    // Now back to frequency domain
    fftw_execute(fwd_filter_plan);

    memcpy(response,buffer_filter,N*sizeof(*response));

    return 0;
}

// Compute an entire Kaiser window
// More efficient than repeatedly calling kaiser(n,M,beta)
int make_kaiser(double * const window,unsigned int const M,double const beta)
{
    assert(window != NULL);
    if(window == NULL)
        return -1;
    // Precompute unchanging partial values
    double const numc = M_PI * beta;
    double const inv_denom = 1. / i0(numc); // Inverse of denominator
    double const pc = 2.0 / (M-1);

    // The window is symmetrical, so compute only half of it and mirror
    // this won't compute the middle value in an odd-length sequence
    for(int n = 0; n < M/2; n++)
    {
        double const p = pc * n  - 1;
        window[M-1-n] = window[n] = i0(numc * sqrtf(1-p*p)) * inv_denom;
    }
    // If sequence length is odd, middle value is unity
    if(M & 1)
        window[(M-1)/2] = 1; // The -1 is actually unnecessary

    return 0;
}

// Modified Bessel function of the 0th kind, used by the Kaiser window
const double i0(double const z)
{
    const double t = (z*z)/4;
    double sum = 1 + t;
    double term = t;
    for(int k = 2; k < 40; k++)
    {
        term *= t/(k*k);
        sum += term;
        if(term < 1e-12 * sum)
            break;
    }
    return sum;
}

void rational_resampler(double * in, int in_size, double * out, int rate, int interpolation_decimation)
{
	if (interpolation_decimation==DECIMATION)
	{
		int index=0;
		for(int i=0;i<in_size;i+=rate)
		{
			*(out+index)=*(in+i);
			index++;
		}
	}
	else if (interpolation_decimation==INTERPOLATION)
	{
		for(int i=0;i<in_size-1;i++)
		{
			for(int j=0;j<rate;j++)
			{
				*(out+i*rate+j)=interpolate_linear(*(in+i),0,*(in+i+1),rate,j);
			}
		}
		for(int j=0;j<rate;j++)
		{
			*(out+(in_size-1)*rate+j)=interpolate_linear(*(in+in_size-2),0,*(in+in_size-1),rate,rate+j);
		}
	}
}

double interpolate_linear(double  a,double a_x,double b,double b_x,double x)
{
	double  return_val;

	return_val=a+(b-a)*(x-a_x)/(b_x-a_x);

	return return_val;
}

//we define one more than needed to cover the boundary of quadrature
#define MAX_PHASE_COUNT (16385)
static int	phase_table[MAX_PHASE_COUNT];
int sampling_freq = 96000;

void vfo_init_phase_table()
{
    for (int i = 0; i < MAX_PHASE_COUNT; i++)
    {
        double d = (M_PI/2) * ((double)i)/((double)MAX_PHASE_COUNT);
        phase_table[i] = (int)(sin(d) * 1073741824.0);
    }
}

void vfo_start(struct vfo *v, int frequency_hz, int start_phase)
{
    v->phase_increment = (frequency_hz * 65536) / sampling_freq;
    v->phase = start_phase;
    v->freq_hz = frequency_hz;
}

int vfo_read(struct vfo *v)
{
    int i = 0;
    if (v->phase < 16384)
        i = phase_table[v->phase];
    else if (v->phase < 32768)
        i = phase_table[32767 - v->phase];
    else if (v->phase < 49152)
        i =  -phase_table[v->phase - 32768];
    else
        i = -phase_table[65535- v->phase];

//increment the phase and modulo-65536
    v->phase += v->phase_increment;
    v->phase &= 0xffff;

    return i;
}


// TODO: We need to define our reference and implement the ALC in the rx alsa input
//
// TODO: Add to DSP a de-noise function with libspecbleach
//       https://github.com/Rhizomatica/libspecbleach/blob/main/example/adenoiser_demo.c

void dsp_process_agc()
{
    static float input_buffer[1024]; // block_size == 1024
    static float output_buffer[1024]; // block_size == 1024

    int block_size = MAX_BINS / 2;

    // TODO: define the SLOW, MEDIUM and FAST AGCs
    short hang_time=400; // in frames
    float reference=0.2; // reference level
    float attack_rate=0.02;
    float decay_rate=0.0002;
    float max_gain=20; // in db
    short attack_wait=0;
    float filter_alpha=0.999;//0.001;

    static float last_gain=1.0;

    for (int i = 0; i < MAX_BINS / 2; i++)
    {
        input_buffer[i] = (float) cimag(fft_time[i + (MAX_BINS / 2)]);
    }

    last_gain = agc_ff(input_buffer, output_buffer, block_size, reference, attack_rate, decay_rate, max_gain, hang_time, attack_wait, filter_alpha, last_gain);

    for (int i = 0; i < MAX_BINS / 2; i++)
    {
        __imag__ (fft_time[i + (MAX_BINS / 2)]) = (double) output_buffer[i];
    }

}

// old tests for AGC
#if 0

void dsp_process_agc_old_fastagc()
{
    static bool first_call = true;
    static fastagc_ff_t input;
    static float* agc_output_buffer;

    // TODO: this is just a test... move these init stuff to appropriate _init and _free functions...
    if (first_call)
    {
        input.input_size = MAX_BINS/2;

        input.reference = 0.2;

        input.buffer_1 = (float*) calloc(input.input_size,sizeof(float));
        input.buffer_2 = (float*) calloc(input.input_size,sizeof(float));
        input.buffer_input = (float*) malloc(sizeof(float)*input.input_size);
        agc_output_buffer = (float*) malloc(sizeof(float)*input.input_size);

        first_call = false;
    }

    for (int i = 0; i < MAX_BINS / 2; i++)
    {
        input.buffer_input[i] = (float) cimag(fft_time[i + (MAX_BINS / 2)]);
    }
    fastagc_ff(&input, agc_output_buffer);
    for (int i = 0; i < MAX_BINS / 2; i++)
    {
        __imag__ (fft_time[i + (MAX_BINS / 2)]) = (double) agc_output_buffer[i];
    }

}

void dsp_process_agc_old()
{
    double signal_strength, agc_gain_should_be;
    int agc_speed;

    static double agc_gain = 0.0;
    static int agc_loop = 0;

    _Atomic uint16_t agc = radio_h_dsp->profiles[radio_h_dsp->profile_active_idx].agc;
    switch (agc)
    {
    case AGC_OFF:
        return;
        break;
    case AGC_SLOW:
        agc_speed = 100;
        break;
    case AGC_MEDIUM:
        agc_speed = 33;
        break;
    case AGC_FAST:
        agc_speed = 10;
        break;
    default:
        return;
    }

    //find the peak signal amplitude
    signal_strength = 0.0;
    for (int i = 0; i < MAX_BINS/2; i++)
    {
        double s = cimag(fft_time[i+(MAX_BINS/2)]);
        if (signal_strength < s)
            signal_strength = s;
    }

    if (signal_strength < 0.00001 && signal_strength > -0.00001)
        agc_gain_should_be = 1;
    else
        agc_gain_should_be = 0.5 / signal_strength;

    double agc_ramp = 0.0;

    if (agc_gain_should_be < agc_gain)
    {
        agc_gain = agc_gain_should_be;
        agc_loop = agc_speed;
    }
    else if (agc_loop <= 0)
    {
        agc_ramp = (agc_gain_should_be - agc_gain) / (MAX_BINS/2);
    }

    for (int i = 0; i < MAX_BINS/2; i++)
    {
        __imag__ (fft_time[i+(MAX_BINS/2)]) *= agc_gain;
    }
    agc_gain += agc_ramp;

    agc_loop--;

    return;
}

#endif
