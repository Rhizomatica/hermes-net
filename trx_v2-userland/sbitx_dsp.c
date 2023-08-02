/* sBitx controller
 * Copyright (C) 2023 Rhizomatica
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
#include "sbitx_dsp.h"
#include "fir_filter.h"
#include "interpolation.h"

// this is just for sdr.h declaration
#include <fftw3.h>
#include <complex.h>
#include "sdr.h"

// we read the mode from here
extern struct rx *rx_list;
extern snd_pcm_format_t format;
extern double tx_amp;


struct st_fir_filter_coefficients *FIR_filter;

static uint32_t cw_sample = 0;

double measure_signal_stregth(double *in, int nItems)
{
	double signal_stregth=0;
	double signal_stregth_dbm=0;

	for(int i=0;i<nItems;i++)
	{
		signal_stregth+=pow(*(in+i),2);
	}
	signal_stregth/=nItems;

	signal_stregth_dbm=10.0*log10((signal_stregth)/0.001);

	return signal_stregth_dbm;
}

// - signal_input: 96 kHz mono from radio, get the "slice" between 24 kHz and 27 kHz (USB) or 21 kHz to 24 kHz (LSB), and bring this slice to 0 and 3 kHz
// - out output_speaker: 96 kHz mono output for speaker
// - out output_loopback: 48 kHz stereo for loopback input
// - out output_tx: NULL buffer
// - block_size: number of samples
void dsp_process_rx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size)
{
    int32_t *input_signal = (int32_t *) signal_input;
    int32_t *output_loopback_int = (int32_t *) output_loopback;
    int32_t *output_speaker_int = (int32_t *) output_speaker;


    double input_signal_f[block_size];
	double _Complex c_baseband_data[block_size];
	double _Complex c_baseband_data_filtered[block_size];

	double c_intermediate_data[block_size];
	double c_intermediate_data_decimated[block_size/2];
//	int32_t c_intermediate_data_decimated_stereo[block_size];


    for (uint32_t i = 0; i < block_size; i++)
    {
        input_signal_f[i] = (double) input_signal[i] / (double) 2147483647.0; // 2^31 - 1
        // input_signal_f[i] = (double) input_signal[i] / (double) 200000000.0; // this is what reference sofftware does....
    }

//    if (rx_list->mode == MODE_USB)
    if (rx_list->mode == MODE_USB || rx_list->mode == MODE_LSB || rx_list->mode == MODE_CW)
    {
        double carrier_amplitude=sqrt(2);

        double passband_samp_freq=96000;
        double passband_start_freq=24000;
        double passband_end_freq=27000;

        double intermediate_samp_freq=48000;
        double intermediate_carrier_frequency=(passband_end_freq-passband_start_freq)/2;

        double passband_samp_interval=1.0/passband_samp_freq;
        double passband_carrier_frequency=(passband_start_freq+passband_end_freq)/2.0;

        for(uint32_t i=0;i < block_size;i++)
        {
            __real__ c_baseband_data[i] =input_signal_f[i]*carrier_amplitude*cos(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
            __imag__ c_baseband_data[i] =input_signal_f[i]*carrier_amplitude*sin(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
        }

        fir_filter_apply(FIR_filter, c_baseband_data,c_baseband_data_filtered, block_size);

        for(uint32_t i=0;i<block_size;i++)
        {
            c_intermediate_data[i] =creal(c_baseband_data_filtered[i])*carrier_amplitude*cos(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
            c_intermediate_data[i] +=cimag(c_baseband_data_filtered[i])*carrier_amplitude*sin(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
        }

//        printf("%f dbm\n", measure_signal_stregth (c_intermediate_data, block_size));

        rational_resampler(c_intermediate_data,block_size,c_intermediate_data_decimated,(int)(passband_samp_freq/intermediate_samp_freq),DECIMATION);

        for(uint32_t i=0;i<block_size;i++)
        {
            output_speaker_int[i] = (int32_t) (c_intermediate_data[i] * 2147483647);
        }

        for(uint32_t i=0; i < (block_size / 2);i++)
        {
            output_loopback_int[i*2] = (int32_t) (c_intermediate_data_decimated[i] * 2147483647);
            output_loopback_int[i*2 + 1] = output_loopback[i*2];
        }
        // USB tuning and filtering
    }

#if 0
    else if (rx_list->mode == MODE_LSB)
    {
        // TODO: LSB tuning and filtering
    }
#endif
    if (rx_list->mode == MODE_CW)
        cw_sample = 0;

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
    int32_t *input_signal = (int32_t *) signal_input;
    int32_t *output_tx_int = (int32_t *) output_tx;

    double input_signal_f_48k[block_size/2];
    double input_signal_f[block_size];

	double _Complex c_baseband_data[block_size];
	double _Complex c_baseband_data_filtered[block_size];

    double c_passband_data_tx[block_size];

    double carrier_amplitude=sqrt(2);

    double passband_samp_freq=96000;
    double passband_start_freq=24000;
    double passband_end_freq=27000;

    double intermediate_samp_freq=48000;
    double intermediate_carrier_frequency=(passband_end_freq-passband_start_freq)/2;

    double passband_samp_interval=1.0/passband_samp_freq;
    double passband_carrier_frequency=(passband_start_freq+passband_end_freq)/2.0;

    if (input_is_48k_stereo)
    {
        for (uint32_t i = 0; i < block_size; i += 2)
            input_signal_f_48k[i/2] = (double) input_signal[i] / (double) 2147483647.0;

        rational_resampler(input_signal_f_48k,(int)((double)(block_size)*intermediate_samp_freq/passband_samp_freq),input_signal_f,(int)(passband_samp_freq/intermediate_samp_freq),INTERPOLATION);
//        rational_resampler(input_signal_f_48k, block_size / 2, input_signal_f, 2, INTERPOLATION);
    }
    else
    {
        for (uint32_t i = 0; i < block_size; i++)
            input_signal_f[i] = (double) input_signal[i] / (double) 2147483647.0;
    }


    if (rx_list->mode == MODE_CW)
    {
        double sampling_frequency=96000;
        double sampling_interval= 1.0 / sampling_frequency;
        double carrier_amplitude=sqrt(2);

        double carrier_frequency=1500;

        for(uint32_t i = cw_sample; i < (block_size + cw_sample); i++)
        {
            input_signal_f[i-cw_sample] = carrier_amplitude*cos(2*M_PI*carrier_frequency*(double)i * sampling_interval);
        }
        cw_sample += block_size;
    }


    // USB tuning and filtering
//    if (rx_list->mode == MODE_USB)
    if (rx_list->mode == MODE_USB || rx_list->mode == MODE_LSB || rx_list->mode == MODE_CW)
    {

        for(uint32_t i=0;i < block_size;i++)
        {
            __real__ c_baseband_data[i] = input_signal_f[i]*carrier_amplitude*cos(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
            __imag__ c_baseband_data[i] = input_signal_f[i]*carrier_amplitude*sin(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
        }

        fir_filter_apply(FIR_filter, c_baseband_data,c_baseband_data_filtered, block_size);

        for(uint32_t i=0;i < block_size;i++)
        {
            c_passband_data_tx[i] =creal(input_signal_f[i])*carrier_amplitude*cos(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
            c_passband_data_tx[i] +=cimag(input_signal_f[i])*carrier_amplitude*sin(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
        }

        for(uint32_t i = 0;i < block_size;i++)
        {
            output_tx_int[i] = (int32_t) (c_passband_data_tx[i] * 2147483647) * tx_amp;
        }

    }
#if 0
    else if (rx_list->mode == MODE_LSB)
    {
        // LSB tuning and filtering
    }
#endif


    memset(output_loopback, 0, block_size * (snd_pcm_format_width(format) / 8));
    memset(output_speaker, 0, block_size * (snd_pcm_format_width(format) / 8));

}

void dsp_start()
{
    // just USB for now
    double passband_samp_freq=96000;
    double passband_start_freq=24000;
    double passband_end_freq=27000;

    double intermediate_carrier_frequency=(passband_end_freq-passband_start_freq)/2;

    FIR_filter= fir_filter_design(LPF, BLACKMAN, 200, intermediate_carrier_frequency, passband_samp_freq);

}
