/*
 * sBitx RADEv2 Digital Voice Integration
 *
 * Copyright (C) 2024-2025 Rhizomatica
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
 * RADEv2 - Radio Autoencoder Version 2
 * See: https://github.com/drowe67/radae
 */

#ifndef SBITX_RADAE_H_
#define SBITX_RADAE_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declaration - full definition in sbitx_core.h
struct radio;
typedef struct radio radio;

// Sample rates
#define RADAE_MODEM_RATE     8000   // RADAE modem IQ sample rate
#define RADAE_SPEECH_RATE    16000  // RADAE speech sample rate (for lpcnet)
#define RADIO_SAMPLE_RATE    96000  // sBitx radio sample rate
#define LOOPBACK_SAMPLE_RATE 48000  // Loopback sample rate

// RADEv2 model parameters
#define RADAE_LATENT_DIM     56
#define RADAE_W1_DEC         128

// Buffer sizes
#define RADAE_SPEECH_BUFFER_SIZE   (RADAE_SPEECH_RATE * 2)  // 2 seconds of speech
#define RADAE_MODEM_BUFFER_SIZE    (RADAE_MODEM_RATE * 2)   // 2 seconds of modem IQ

// Feature extraction frame size (10ms @ 16kHz = 160 samples)
#define RADAE_FRAME_SIZE     160
#define RADAE_FEATURES_PER_FRAME 36

// Paths to RADAE resources (relative to radae directory)
#define RADAE_MODEL_PATH         "250725/checkpoints/checkpoint_epoch_200.pth"
#define RADAE_SYNC_MODEL_PATH    "250725a_ml_sync"
#define RADAE_DIR                "../radae"

// RADAE context structure
typedef struct {
    // Subprocess PIDs
    pid_t tx_feature_pid;    // lpcnet_demo -features for TX
    pid_t tx_encoder_pid;    // inference.py for TX
    pid_t rx_decoder_pid;    // rx2.py for RX
    pid_t rx_synth_pid;      // lpcnet_demo -fargan-synthesis for RX

    // Pipe file descriptors for TX path
    int tx_speech_to_features[2];    // speech -> lpcnet_demo -features
    int tx_features_to_encoder[2];   // features -> inference.py
    int tx_encoder_to_modem[2];      // inference.py -> modem IQ

    // Pipe file descriptors for RX path
    int rx_modem_to_decoder[2];      // modem IQ -> rx2.py
    int rx_decoder_to_synth[2];      // features -> lpcnet_demo -fargan-synthesis
    int rx_synth_to_speech[2];       // synthesized speech output

    // Circular buffers for sample rate conversion
    float *tx_speech_buffer;         // 16kHz speech input buffer
    int tx_speech_buffer_write_idx;
    int tx_speech_buffer_read_idx;

    float *tx_modem_buffer;          // 8kHz complex IQ output buffer
    int tx_modem_buffer_write_idx;
    int tx_modem_buffer_read_idx;

    float *rx_modem_buffer;          // 8kHz complex IQ input buffer
    int rx_modem_buffer_write_idx;
    int rx_modem_buffer_read_idx;

    float *rx_speech_buffer;         // 16kHz speech output buffer
    int rx_speech_buffer_write_idx;
    int rx_speech_buffer_read_idx;

    // Thread handles
    pthread_t tx_thread;
    pthread_t rx_thread;

    // Mutex for buffer access
    pthread_mutex_t tx_mutex;
    pthread_mutex_t rx_mutex;

    // Condition variables for signaling
    pthread_cond_t tx_cond;
    pthread_cond_t rx_cond;

    // State flags
    _Atomic bool initialized;
    _Atomic bool tx_running;
    _Atomic bool rx_running;
    _Atomic bool shutdown_requested;

    // Radio handle reference
    radio *radio_h;

    // RADAE directory path
    char radae_dir[256];

} radae_context;

// Initialize RADAE subsystem
bool radae_init(radae_context *ctx, radio *radio_h, const char *radae_dir);

// Shutdown RADAE subsystem
void radae_shutdown(radae_context *ctx);

// Start TX processing (call when PTT is pressed)
bool radae_tx_start(radae_context *ctx);

// Stop TX processing (call when PTT is released)
void radae_tx_stop(radae_context *ctx);

// Start RX processing
bool radae_rx_start(radae_context *ctx);

// Stop RX processing
void radae_rx_stop(radae_context *ctx);

// TX: Feed speech samples (expects 16kHz mono float samples)
// Returns number of samples written
int radae_tx_write_speech(radae_context *ctx, const float *samples, int n_samples);

// TX: Get modem IQ samples (8kHz complex float samples, interleaved I,Q,I,Q...)
// Returns number of complex samples read
int radae_tx_read_modem_iq(radae_context *ctx, float *iq_samples, int max_samples);

// RX: Feed modem IQ samples (8kHz complex float samples, interleaved I,Q,I,Q...)
// Returns number of complex samples written
int radae_rx_write_modem_iq(radae_context *ctx, const float *iq_samples, int n_samples);

// RX: Get speech samples (16kHz mono float samples)
// Returns number of samples read
int radae_rx_read_speech(radae_context *ctx, float *samples, int max_samples);

// Check if TX has modem IQ data available
int radae_tx_modem_available(radae_context *ctx);

// Check if RX has speech data available
int radae_rx_speech_available(radae_context *ctx);

// Sample rate conversion utilities
void resample_96k_to_16k(const double *in, int in_len, float *out, int *out_len);
void resample_16k_to_96k(const float *in, int in_len, double *out, int *out_len);
void resample_48k_to_16k(const double *in, int in_len, float *out, int *out_len);
void resample_16k_to_48k(const float *in, int in_len, double *out, int *out_len);
void resample_96k_to_8k(const double *in, int in_len, float *out, int *out_len);
void resample_8k_to_96k(const float *in, int in_len, double *out, int *out_len);

#endif // SBITX_RADAE_H_
