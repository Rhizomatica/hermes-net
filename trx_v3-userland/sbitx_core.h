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

#define CFG_CORE_PATH "/etc/sbitx/core.ini"
#define CFG_USER_PATH "/etc/sbitx/user.ini"
#define CFG_WEBSOCKET_PATH "/etc/sbitx/web"
#define CFG_SSL_CERT "/etc/ssl/private/hermes.radio.crt"
#define CFG_SSL_KEY "/etc/ssl/private/hermes.key"

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>

#include <iniparser.h>

// around 400ms before cutting the tx in case of high swr
#define REF_PEAK_REMOVAL 10

/* GPIO Pin definitions  // Hardware 40-header numbering commented */
#define ENC1_A    9 // Pin 21
#define ENC1_B   10 // Pin 19
#define ENC1_SW  11 // Pin 23

#define ENC2_A   17 // Pin 11
#define ENC2_B   27 // Pin 13
#define ENC2_SW  22 // Pin 15

#define PTT       4 // Pin 7
#define DASH      5 // Pin 29

#define TX_LINE  23 // Pin 16
#define TX_POWER 16 // Pin 36
#define LPF_A    24 // Pin 18
#define LPF_B    25 // Pin 22
#define LPF_C     8 // Pin 24
#define LPF_D     7 // Pin 26

/* Internal software modes listing */
#define OPERATING_MODE_FULL_VOICE 0 // DSP + IO, radio tx comes from MIC
#define OPERATING_MODE_FULL_LOOPBACK 1 // DSP + IO, radio tx comes from Alsa loopback
#define OPERATING_MODE_CONTROLS_ONLY 2 // just IO, does not open the Alsa device

/* ... yes, this is all done in software. */
#define MODE_LSB 0
#define MODE_USB 1
#define MODE_CW 2

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

    _Atomic uint32_t freq;
    uint16_t operating_mode; // FULL or CONTROLS_ONLY

    // In OPERATING_MODE_CONTROLS_ONLY, most of these will just be ignored...
    _Atomic uint16_t mode; // MODE_*
    uint16_t agc; // AGC_*
    uint16_t compressor; // COMPRESSOR_*

    // Alsa levels
    _Atomic uint32_t mic_level; // 0 - 100
    _Atomic uint32_t rx_level;
    _Atomic uint32_t speaker_level; // 0 - 100
    _Atomic uint32_t tx_level;

    // lpf settings
    uint32_t bpf_low; // band-pass filter settings
    uint32_t bpf_high;

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
    pthread_mutex_t gpio_mutex;
    pthread_mutex_t cfg_mutex;

    // Radio status
    _Atomic uint32_t bfo_frequency;
    _Atomic bool txrx_state; // IN_RX or IN_TX
    _Atomic uint32_t reflected_threshold; // vswr * 10
    _Atomic bool swr_protection_enabled;

    // front panel controls and status
    encoder enc_a;
    encoder enc_b;

    _Atomic int32_t volume_ticks;
    _Atomic int32_t tuning_ticks;

    // knob frequency step size
    _Atomic uint32_t step_size;

    _Atomic uint32_t knob_a_pressed;
    _Atomic uint32_t knob_b_pressed;

    _Atomic bool key_down; // this is the ptt button
    _Atomic bool dash_down;

    // raw values from read from the ATTiny 10bit ADC over I2C
    _Atomic uint32_t fwd_power;
    _Atomic uint32_t ref_power;

    _Atomic uint32_t bridge_compensation;

    _Atomic bool enable_websocket;
    _Atomic bool enable_shm_control; // this is needed for sbitx_client

    // some informational fields
    _Atomic uint32_t serial_number;
    _Atomic bool system_is_connected;  // VARA connection status
    _Atomic bool system_is_ok;  // means uucp is up and running

    // read just at load, no need for atomic
    power_settings band_power[MAX_CAL_BANDS];
    uint32_t band_power_count;

    // profile variables
    _Atomic uint32_t profile_active_idx;
    _Atomic int32_t profile_timeout; // set to -1 to disable return to "default" profile timeout, or set to the number of seconds for going to the default in case of idle (or what?)
    _Atomic uint32_t profile_default_idx;
    _Atomic uint32_t profiles_count;
    radio_profile profiles[MAX_RADIO_PROFILES];

    dictionary *cfg_core;
    _Atomic bool cfg_core_dirty;
    dictionary *cfg_user;
    _Atomic bool cfg_user_dirty;
    _Atomic bool send_ws_update;
} radio;


// init / shutdown functions
bool hw_init(radio *radio_h, pthread_t *hw_tids);
bool hw_shutdown(radio *radio_h, pthread_t *hw_tids);

// hw io thread
void *hw_thread(void *radio_h_v);
void io_tick(radio *radio_h);

void set_frequency(radio *radio_h, uint32_t frequency, uint32_t profile);
void set_mode(radio *radio_h, uint16_t mode, uint32_t profile);
void set_bfo(radio *radio_h, uint32_t frequency);
void set_reflected_threshold(radio *radio_h, uint32_t ref_threshold);
void set_speaker_volume(radio *radio_h, uint32_t speaker_level, uint32_t profile);

// TX/RX switch
void tr_switch(radio *radio_h, bool txrx_state);

// disconnect all LPFs
void lpf_off(radio *radio_h);
// set appropriate LPF according to the frequency
void lpf_set(radio *radio_h);

// fwd, ref and swr measurements functions
// update_power_measurements() calls the i2c power readings
bool update_power_measurements(radio *radio_h);
uint32_t get_fwd_power(radio *radio_h);
uint32_t get_ref_power(radio *radio_h);
uint32_t get_swr(radio *radio_h);
void swr_protection_check(radio *radio_h);

// auxiliary functions for timer functionality
void wait_next_activation(void);
int start_periodic_timer(uint64_t offset);

#endif // SBITX_CORE_H_
