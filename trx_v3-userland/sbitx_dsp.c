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
#include "sbitx_dsp.h"
#include "fir_filter.h"
#include "interpolation.h"

extern snd_pcm_format_t format;
//extern double tx_amp;


//struct st_fir_filter_coefficients *FIR_filter;

#define double2int(i, d) \
    {double t = ((d) + 6755399441055744.0); i = *((int32_t *)(&t));}

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

void dsp_start()
{


}
