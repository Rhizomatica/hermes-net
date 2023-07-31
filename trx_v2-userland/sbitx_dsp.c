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

// this is just for sdr.h declaration
#include <fftw3.h>
#include <complex.h>
#include "sdr.h"

// we read the mode from here
extern struct rx *rx_list;

// - signal_input: 96 kHz mono from radio, get the "slice" between 24 kHz and 27 kHz (USB) or 21 kHz to 24 kHz (LSB), and bring this slice to 0 and 3 kHz
// - out output_speaker: 96 kHz mono output for speaker
// - out output_loopback: 48 kHz stereo for loopback input
// - out output_tx: NULL buffer
// - block_size: number of samples
void dsp_process_rx(uint8_t *signal_input, uint8_t *output_speaker, uint8_t *output_loopback, uint8_t *output_tx, uint32_t block_size)
{
    int32_t *input_signal = (int32_t *) signal_input;
    double input_signal_f[block_size];


    for (uint32_t i = 0; i < block_size; i++)
    {
        input_signal_f[i] = (double) input_signal[i] / (double) 2147483647.0; // 2^31 - 1
        // input_signal_f[i] = (double) input_signal[i] / (double) 200000000.0; // this is what reference sofftware does....
    }

    if (rx_list->mode == MODE_USB)
    {
        // USB tuning and filtering
    }
    else if (rx_list->mode == MODE_LSB)
    {
        // LSB tuning and filtering
    }


    // output_tx: NULL
    // output_loopback: 2 channels (equal) 2/1 downsampled
    // output_speaker: 1 channel, no re-sampling
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
    double input_signal_f[block_size];


    if (input_is_48k_stereo)
    {
        for (uint32_t i = 0; i < block_size; i += 2)
            input_signal_f[i] = (double) input_signal[i] / (double) 2147483647.0;
        for (uint32_t i = 0; i < (block_size-2); i++)
            input_signal_f[i+1] = (input_signal_f[i] + input_signal_f[i+2]) / 2;
        input_signal_f[block_size-1] = input_signal_f[block_size-2];
    }
    else
    {
        for (int i = 0; i < block_size; i++)
            input_signal_f[i] = (double) input_signal[i] / (double) 2147483647.0;
    }

    if (rx_list->mode == MODE_USB)
    {
        // USB tuning and filtering
    }
    else if (rx_list->mode == MODE_LSB)
    {
        // LSB tuning and filtering
    }

    // output_loopback: NULL
    // output_speaker: NILL
    // output_tx: USB/LSB signal, no resampling

}
