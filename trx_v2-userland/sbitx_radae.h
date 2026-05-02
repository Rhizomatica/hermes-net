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
 * RADE digital voice integration for RADEv1 and RADEv2.
 */

#ifndef SBITX_RADAE_H_
#define SBITX_RADAE_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "sbitx_core.h"

// Sample rates
#define RADAE_MODEM_RATE     8000   // RADAE modem IQ sample rate
#define RADAE_SPEECH_RATE    16000  // RADAE speech sample rate (for lpcnet)


// Buffer sizes
#define RADAE_SPEECH_BUFFER_SIZE   (RADAE_SPEECH_RATE * 2)  // 2 seconds of speech
#define RADAE_MODEM_BUFFER_SIZE    (RADAE_MODEM_RATE * 2)   // 2 seconds of modem IQ

// Feature extraction frame size (10ms @ 16kHz = 160 samples)
#define RADAE_FRAME_SIZE     160

// RADE version selection
#define RADAE_VERSION_V1         "v1"
#define RADAE_VERSION_V2         "v2"
#define RADAE_VERSION_DEFAULT    RADAE_VERSION_V2

// RADEv2 resources (relative to /opt/radae)
#define RADAE_MODEL_PATH_V2      "250725/checkpoints/checkpoint_epoch_200.pth"
#define RADAE_SYNC_MODEL_PATH_V2 "250725a_ml_sync"
#define RADAE_RX_BINARY_PATH_V2  "build/src/radae_rx_v2"
#define RADAE_TX_BINARY_PATH_V2  "build/src/radae_tx_v2"
#define RADAE_DIR_V2             "/opt/radae"

// RADEv1 resources (relative to /opt/radae_nopy)
#define RADAE_MODEL_PATH_V1      "builtin"
#define RADAE_RX_BINARY_PATH_V1  "build/src/radae_rx"
#define RADAE_TX_BINARY_PATH_V1  "build/src/radae_tx"
#define RADAE_DIR_V1             "/opt/radae_nopy"

// The native TX binary writes its own PID here at startup; the TX thread
// reads it back so radae_tx_emit_eoo can deliver SIGUSR1 directly (the
// fork+exec target is /bin/sh, so tx_encoder_pid points at the shell,
// not the encoder process).  This contract exists only on RADEv2.
#define RADAE_TX_PID_FILE        "/tmp/radae_tx.pid"

// RADAE context structure
typedef struct {
    // Subprocess PIDs
    pid_t tx_encoder_pid;         // TX encoder pipeline shell
    pid_t tx_encoder_eoo_pid;     // RADEv2 child PID (target for SIGUSR1 EOO)
    pid_t rx_decoder_pid;    // RX decoder pipeline

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
    _Atomic bool tx_eoo_only;    // stop feeding stdin and drain TX output only
    _Atomic bool rx_running;
    _Atomic bool shutdown_requested;

    // Radio handle reference
    radio *radio_h;

    // RADAE directory path
    char radae_dir[256];
    char rade_version[8];

} radae_context;

// Initialize RADAE subsystem
bool radae_init(radae_context *ctx, radio *radio_h);

// Shutdown RADAE subsystem
void radae_shutdown(radae_context *ctx);

// Start TX processing (call when PTT is pressed)
bool radae_tx_start(radae_context *ctx);

// Stop TX processing (call when PTT is released)
void radae_tx_stop(radae_context *ctx);

// Ask the TX encoder (radae_tx_v2) to emit a V2 end-of-over frame
// (6 pend_cp symbols).  Non-blocking: sends SIGUSR1 to the cached
// encoder PID and returns.  The caller is responsible for giving the
// IQ time to flow through the modem buffer -> DSP -> DAC before the
// PA drive is dropped.  Returns false if the encoder PID isn't known
// yet (startup race, or subprocess died).
bool radae_tx_emit_eoo(radae_context *ctx);

// Start RX processing
bool radae_rx_start(radae_context *ctx);

// Stop RX processing
void radae_rx_stop(radae_context *ctx);

// Flush RX buffers (call on TX->RX transition to discard stale data)
void radae_rx_flush(radae_context *ctx);

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

// Sample rate conversion utilities
void resample_96k_to_16k(const double *in, int in_len, float *out, int *out_len);
void resample_16k_to_96k(const float *in, int in_len, double *out, int *out_len);
void resample_48k_to_16k(const double *in, int in_len, float *out, int *out_len);
void resample_96k_to_8k(const double *in, int in_len, float *out, int *out_len);
void resample_8k_to_96k(const float *in, int in_len, double *out, int *out_len);

// True when RADAE_DEBUG env var was set to 1/true at radae_init time.
bool radae_is_debug(void);

// Amplitude instrumentation: tracks |peak| and mean |val| over a 1 s window
// for a stream of int64-able samples (so it works for int32, double, float).
// Gated by radae_is_debug(); silent in production.
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#define RADAE_AMPL_LOG(tag, val) do {                                          \
    if (radae_is_debug()) {                                                    \
        static double   _ap_sum  = 0.0;                                        \
        static double   _ap_peak = 0.0;                                        \
        static uint64_t _ap_n    = 0;                                          \
        static struct timespec _ap_t0 = {0, 0};                                \
        struct timespec _ap_now;                                               \
        clock_gettime(CLOCK_MONOTONIC, &_ap_now);                              \
        if (_ap_t0.tv_sec == 0 && _ap_t0.tv_nsec == 0) _ap_t0 = _ap_now;       \
        double _ap_v = fabs((double)(val));                                    \
        _ap_sum += _ap_v;                                                      \
        if (_ap_v > _ap_peak) _ap_peak = _ap_v;                                \
        _ap_n += 1;                                                            \
        double _ap_dt = (_ap_now.tv_sec  - _ap_t0.tv_sec) +                    \
                        (_ap_now.tv_nsec - _ap_t0.tv_nsec) / 1e9;              \
        if (_ap_dt >= 1.0) {                                                   \
            double _ap_mean = _ap_n ? _ap_sum / (double)_ap_n : 0.0;           \
            fprintf(stderr,                                                    \
                "RADAE ampl [%s]: peak=%.3g mean=%.3g n=%llu\n",               \
                (tag), _ap_peak, _ap_mean,                                     \
                (unsigned long long)_ap_n);                                    \
            _ap_sum  = 0.0;                                                    \
            _ap_peak = 0.0;                                                    \
            _ap_n    = 0;                                                      \
            _ap_t0   = _ap_now;                                                \
        }                                                                      \
    }                                                                          \
} while (0)

#endif // SBITX_RADAE_H_
