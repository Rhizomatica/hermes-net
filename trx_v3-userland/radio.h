/* sBitx core
 * Copyright (C) 2023 Rhizomatica
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

#ifndef RADIO_H_
#define RADIO_H_

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#define OPERATING_MODE_FULL_VOICE 0 // DSP + IO, radio tx comes from MIC
#define OPERATING_MODE_FULL_LOOPBACK 1 // DSP + IO, radio tx comes from Alsa loopback
#define OPERATING_MODE_CONTROLS_ONLY 2 // just IO, does not open the Alsa device

#define MODE_LSB 0
#define MODE_USB 1

#define AGC_OFF 0
#define AGC_SLOW 0
#define AGC_MED 0
#define AGC_FAST 0

#define COMPRESSION_OFF 0
#define COMPRESSION_ON 1

typedef struct {

    uint32_t frequency;
    uint16_t operating_mode; // FULL or CONTROLS_ONLY

    // In OPERATING_MODE_CONTROLS_ONLY, most of these will just be ignored...
    uint16_t mode; // MODE_*
    uint16_t agc; // AGC_*
    uint16_t compression; // COMPRESSION_*

    // Alsa levels
    uint16_t mic_level; // 0 - 100
    uint16_t rx_level;
    uint16_t speaker_level; // 0 - 100
    uint16_t tx_level;

    uint32_t bpf_low; // band-pass filter settings
    uint32_t bpf_high;

    uint32_t step_size;

    // These are switche that can enable/disable interaction with the devices
    bool enable_knobs;
    bool enable_ptt;

    // do we need this?
    bool is_default;


    // oque fazer com o knob de frequencia no painel do radio?
    // bloquear o botao de freq quando tiver em digital? sim!
    // websocket interface
    // operating mode change
    // connected_status
    // digital_frequency
    // analog_frequency
    // frequency
    // operating mode

    // UI fonia - soh vai mudar a analog_frequency
    // radio config - voice analog frequency / digital data frequency
    // radio config - operating mode (analog / digital)
    // oque fazer quando tiver ptt quando tiver no modo digital



//    int serial_number;
} radio_profile;



#endif // RADIO_H_
