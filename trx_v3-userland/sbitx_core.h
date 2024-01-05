/* sBitx control daemon - HERMES
 *
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

#ifndef SBITX_CORE_H_
#define SBITX_CORE_H_

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include <iniparser.h>

/* Pin definitions following WirinpiPi numbering // GPIO numbering commented */
#define ENC1_A  13  // GPIO 17 // Pin 11
#define ENC1_B  12  // GPIO 27 // Pin 13
#define ENC1_SW 14  // GPIO 22 // Pin 15

#define ENC2_A   0  // GPIO 10 // Pin 19
#define ENC2_B   2  // GPIO 9  // Pin 21
#define ENC2_SW  3  // GPIO 11 // Pin 23

#define PTT      7  // GPIO  4 // Pin 7
#define DASH    21  // GPIO  5 // Pin 29

#define TX_LINE 4   // GPIO 23 // Pin 16
#define TX_POWER 27 // GPIO 16 // Pin 36
#define LPF_A 5     // GPIO 24 // Pin 18
#define LPF_B 6     // GPIO 25 // Pin 22
#define LPF_C 10    // GPIO  8 // Pin 24
#define LPF_D 11    // GPIO  7 // Pin 26

/* Internal software modes listing */
#define OPERATING_MODE_FULL_VOICE 0 // DSP + IO, radio tx comes from MIC
#define OPERATING_MODE_FULL_LOOPBACK 1 // DSP + IO, radio tx comes from Alsa loopback
#define OPERATING_MODE_CONTROLS_ONLY 2 // just IO, does not open the Alsa device

/* ... yes, this is all done in software. */
#define MODE_LSB 0
#define MODE_USB 1

#define AGC_OFF 0
#define AGC_SLOW 1
#define AGC_MEDIUM 2
#define AGC_FAST 3

#define COMPRESSOR_OFF 0
#define COMPRESSOR_ON 1

/* tx/rx states */
#define IN_RX 0
#define IN_TX 1

/* Encoder speed defines */
#define ENC_FAST 1
#define ENC_SLOW 5

/* Maximum number of radio profiles */
#define MAX_RADIO_PROFILES 4

/* Maximum number of calibration bands */
#define MAX_CAL_BANDS 16

//encoder state variables
typedef struct
{
	int pin_a,  pin_b;
	int speed;
	int prev_state;
	int history;
} encoder;

typedef struct {
	int f_start;
	int f_stop;
	double scale;
} power_settings;

typedef struct {

    uint32_t freq;
    uint16_t operating_mode; // FULL or CONTROLS_ONLY

    // In OPERATING_MODE_CONTROLS_ONLY, most of these will just be ignored...
    uint16_t mode; // MODE_*
    uint16_t agc; // AGC_*
    uint16_t compressor; // COMPRESSOR_*

    // Alsa levels
    uint16_t mic_level; // 0 - 100
    uint16_t rx_level;
    uint16_t speaker_level; // 0 - 100
    uint16_t tx_level;

    // lpf settings
    uint32_t bpf_low; // band-pass filter settings
    uint32_t bpf_high;

    // knob frequency step size
    uint32_t step_size;

    // These are switche that can enable/disable interaction with the devices
    bool enable_knob_volume;
    bool enable_knob_frequency;
    bool enable_ptt;

    // do we need this?
    bool is_default;

} radio_profile;


// radio variables
typedef struct
{
    // I2C
    char i2c_device[64];
    int i2c_bus;
    pthread_mutex_t i2c_mutex;

    // Radio status
    _Atomic uint32_t bfo_frequency;
    _Atomic bool txrx_state; // IN_RX or IN_TX
    uint16_t reflected_threshold; // vswr * 10
    bool swr_protection_enabled;

    // front panel controls and status
    encoder enc_a;
    encoder enc_b;

    _Atomic int32_t volume_ticks;
    _Atomic int32_t tuning_ticks;

    _Atomic uint32_t knob_a_pressed;
    _Atomic uint32_t knob_b_pressed;

    _Atomic bool key_down; // this is the ptt button
    _Atomic bool dash_down;

    // raw values from read from the ATTiny 10bit ADC over I2C
    _Atomic uint32_t fwd_power;
    _Atomic uint32_t ref_power;

    _Atomic uint32_t bridge_compensation;

    // "used once" variables... dont need to be atomic
    bool enable_websocket;
    bool enable_shm_control; // this is needed for sbitx_client

    // some informational fields
    uint32_t serial_number;
    _Atomic bool system_is_connected;  // VARA connection status
    _Atomic bool system_is_ok;  // means uucp is up and running

    power_settings band_power[MAX_CAL_BANDS];
    uint32_t band_power_count;

    // profile variables
    _Atomic uint32_t profile_active_idx;
    _Atomic int32_t profile_timeout; // set to -1 to disable return to "default" profile timeout, or set to the number of seconds for going to the default in case of idle (or what?)
    _Atomic uint32_t profile_default_idx;
    _Atomic uint32_t profiles_count;
    radio_profile profiles[MAX_RADIO_PROFILES];

    dictionary *cfg_core;
    dictionary *cfg_user;

} radio;



void hw_init(radio *radio_h);
void hw_shutdown(radio *radio_h);

void set_frequency(radio *radio_h, uint32_t frequency);
void set_bfo(radio *radio_h, uint32_t frequency);
void tr_switch(radio *radio_h, bool txrx_state);

// disconnect all LPFs
void lpf_off(radio *radio_h);
// set appropriate LPF according to the frequency
void lpf_set(radio *radio_h);

// fwd, ref and swr measurements functions
// call update_power_measurements for a reading
bool update_power_measurements(radio *radio_h);
uint32_t get_fwd_power(radio *radio_h);
uint32_t get_ref_power(radio *radio_h);
uint32_t get_swr(radio *radio_h);

#endif // SBITX_CORE_H_
