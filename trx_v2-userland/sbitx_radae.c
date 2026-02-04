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
 * Uses subprocess pipelines with Python scripts for encoding/decoding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <math.h>

#include "sbitx_core.h"
#include "sbitx_radae.h"

// Helper macros for circular buffer operations
#define BUFFER_SIZE(write_idx, read_idx, max_size) \
    (((write_idx) - (read_idx) + (max_size)) % (max_size))

#define BUFFER_FREE(write_idx, read_idx, max_size) \
    ((max_size) - 1 - BUFFER_SIZE(write_idx, read_idx, max_size))

// TX thread: reads from speech buffer, writes to modem buffer via RADAE pipeline
static void *radae_tx_thread(void *arg);

// RX thread: reads from modem buffer, writes to speech buffer via RADAE pipeline
static void *radae_rx_thread(void *arg);

bool radae_init(radae_context *ctx, radio *radio_h, const char *radae_dir)
{
    memset(ctx, 0, sizeof(radae_context));
    
    ctx->radio_h = radio_h;
    strncpy(ctx->radae_dir, radae_dir, sizeof(ctx->radae_dir) - 1);
    
    // Initialize mutexes and condition variables
    pthread_mutex_init(&ctx->tx_mutex, NULL);
    pthread_mutex_init(&ctx->rx_mutex, NULL);
    pthread_cond_init(&ctx->tx_cond, NULL);
    pthread_cond_init(&ctx->rx_cond, NULL);
    
    // Allocate circular buffers
    // TX speech buffer: 16kHz mono float samples
    ctx->tx_speech_buffer = (float *)calloc(RADAE_SPEECH_BUFFER_SIZE, sizeof(float));
    if (!ctx->tx_speech_buffer) {
        fprintf(stderr, "RADAE: Failed to allocate TX speech buffer\n");
        goto cleanup_mutex;
    }
    
    // TX modem buffer: 8kHz complex IQ (interleaved float pairs)
    ctx->tx_modem_buffer = (float *)calloc(RADAE_MODEM_BUFFER_SIZE * 2, sizeof(float));
    if (!ctx->tx_modem_buffer) {
        fprintf(stderr, "RADAE: Failed to allocate TX modem buffer\n");
        goto cleanup_tx_speech;
    }
    
    // RX modem buffer: 8kHz complex IQ (interleaved float pairs)
    ctx->rx_modem_buffer = (float *)calloc(RADAE_MODEM_BUFFER_SIZE * 2, sizeof(float));
    if (!ctx->rx_modem_buffer) {
        fprintf(stderr, "RADAE: Failed to allocate RX modem buffer\n");
        goto cleanup_tx_modem;
    }
    
    // RX speech buffer: 16kHz mono float samples
    ctx->rx_speech_buffer = (float *)calloc(RADAE_SPEECH_BUFFER_SIZE, sizeof(float));
    if (!ctx->rx_speech_buffer) {
        fprintf(stderr, "RADAE: Failed to allocate RX speech buffer\n");
        goto cleanup_rx_modem;
    }
    
    ctx->initialized = true;
    ctx->shutdown_requested = false;
    
    fprintf(stderr, "RADAE: Initialized with radae_dir=%s\n", ctx->radae_dir);
    
    return true;

cleanup_rx_modem:
    free(ctx->rx_modem_buffer);
cleanup_tx_modem:
    free(ctx->tx_modem_buffer);
cleanup_tx_speech:
    free(ctx->tx_speech_buffer);
cleanup_mutex:
    pthread_mutex_destroy(&ctx->tx_mutex);
    pthread_mutex_destroy(&ctx->rx_mutex);
    pthread_cond_destroy(&ctx->tx_cond);
    pthread_cond_destroy(&ctx->rx_cond);
    return false;
}

void radae_shutdown(radae_context *ctx)
{
    if (!ctx->initialized)
        return;
    
    ctx->shutdown_requested = true;
    
    // Stop TX and RX if running
    radae_tx_stop(ctx);
    radae_rx_stop(ctx);
    
    // Cleanup
    pthread_mutex_destroy(&ctx->tx_mutex);
    pthread_mutex_destroy(&ctx->rx_mutex);
    pthread_cond_destroy(&ctx->tx_cond);
    pthread_cond_destroy(&ctx->rx_cond);
    
    free(ctx->tx_speech_buffer);
    free(ctx->tx_modem_buffer);
    free(ctx->rx_modem_buffer);
    free(ctx->rx_speech_buffer);
    
    ctx->initialized = false;
    fprintf(stderr, "RADAE: Shutdown complete\n");
}

bool radae_tx_start(radae_context *ctx)
{
    if (!ctx->initialized || ctx->tx_running)
        return false;
    
    ctx->tx_running = true;
    ctx->tx_speech_buffer_write_idx = 0;
    ctx->tx_speech_buffer_read_idx = 0;
    ctx->tx_modem_buffer_write_idx = 0;
    ctx->tx_modem_buffer_read_idx = 0;
    
    // Start TX processing thread
    if (pthread_create(&ctx->tx_thread, NULL, radae_tx_thread, ctx) != 0) {
        fprintf(stderr, "RADAE: Failed to create TX thread\n");
        ctx->tx_running = false;
        return false;
    }
    
    fprintf(stderr, "RADAE TX: Started\n");
    return true;
}

void radae_tx_stop(radae_context *ctx)
{
    if (!ctx->tx_running)
        return;
    
    ctx->tx_running = false;
    
    // Signal the thread to wake up
    pthread_cond_signal(&ctx->tx_cond);
    
    // Wait for thread to finish
    pthread_join(ctx->tx_thread, NULL);
    
    // Kill any subprocess
    if (ctx->tx_encoder_pid > 0) {
        kill(ctx->tx_encoder_pid, SIGTERM);
        waitpid(ctx->tx_encoder_pid, NULL, 0);
        ctx->tx_encoder_pid = 0;
    }
    
    fprintf(stderr, "RADAE TX: Stopped\n");
}

bool radae_rx_start(radae_context *ctx)
{
    if (!ctx->initialized || ctx->rx_running)
        return false;
    
    ctx->rx_running = true;
    ctx->rx_modem_buffer_write_idx = 0;
    ctx->rx_modem_buffer_read_idx = 0;
    ctx->rx_speech_buffer_write_idx = 0;
    ctx->rx_speech_buffer_read_idx = 0;
    
    // Start RX processing thread
    if (pthread_create(&ctx->rx_thread, NULL, radae_rx_thread, ctx) != 0) {
        fprintf(stderr, "RADAE: Failed to create RX thread\n");
        ctx->rx_running = false;
        return false;
    }
    
    fprintf(stderr, "RADAE RX: Started\n");
    return true;
}

void radae_rx_stop(radae_context *ctx)
{
    if (!ctx->rx_running)
        return;
    
    ctx->rx_running = false;
    
    // Signal the thread to wake up
    pthread_cond_signal(&ctx->rx_cond);
    
    // Wait for thread to finish
    pthread_join(ctx->rx_thread, NULL);
    
    // Kill any subprocess
    if (ctx->rx_decoder_pid > 0) {
        kill(ctx->rx_decoder_pid, SIGTERM);
        waitpid(ctx->rx_decoder_pid, NULL, 0);
        ctx->rx_decoder_pid = 0;
    }
    
    fprintf(stderr, "RADAE RX: Stopped\n");
}

int radae_tx_write_speech(radae_context *ctx, const float *samples, int n_samples)
{
    if (!ctx->tx_running || !samples || n_samples <= 0)
        return 0;
    
    pthread_mutex_lock(&ctx->tx_mutex);
    
    int free_space = BUFFER_FREE(ctx->tx_speech_buffer_write_idx, 
                                  ctx->tx_speech_buffer_read_idx, 
                                  RADAE_SPEECH_BUFFER_SIZE);
    int to_write = (n_samples < free_space) ? n_samples : free_space;
    
    for (int i = 0; i < to_write; i++) {
        ctx->tx_speech_buffer[ctx->tx_speech_buffer_write_idx] = samples[i];
        ctx->tx_speech_buffer_write_idx = (ctx->tx_speech_buffer_write_idx + 1) % RADAE_SPEECH_BUFFER_SIZE;
    }
    
    pthread_cond_signal(&ctx->tx_cond);
    pthread_mutex_unlock(&ctx->tx_mutex);
    
    return to_write;
}

int radae_tx_read_modem_iq(radae_context *ctx, float *iq_samples, int max_samples)
{
    if (!ctx->tx_running || !iq_samples || max_samples <= 0)
        return 0;
    
    pthread_mutex_lock(&ctx->tx_mutex);
    
    // max_samples is number of complex samples, buffer stores interleaved I,Q
    int available = BUFFER_SIZE(ctx->tx_modem_buffer_write_idx, 
                                 ctx->tx_modem_buffer_read_idx, 
                                 RADAE_MODEM_BUFFER_SIZE * 2) / 2;
    int to_read = (max_samples < available) ? max_samples : available;
    
    for (int i = 0; i < to_read * 2; i++) {
        iq_samples[i] = ctx->tx_modem_buffer[ctx->tx_modem_buffer_read_idx];
        ctx->tx_modem_buffer_read_idx = (ctx->tx_modem_buffer_read_idx + 1) % (RADAE_MODEM_BUFFER_SIZE * 2);
    }
    
    pthread_mutex_unlock(&ctx->tx_mutex);
    
    return to_read;
}

int radae_rx_write_modem_iq(radae_context *ctx, const float *iq_samples, int n_samples)
{
    if (!ctx->rx_running || !iq_samples || n_samples <= 0)
        return 0;
    
    pthread_mutex_lock(&ctx->rx_mutex);
    
    // n_samples is number of complex samples
    int free_space = BUFFER_FREE(ctx->rx_modem_buffer_write_idx, 
                                  ctx->rx_modem_buffer_read_idx, 
                                  RADAE_MODEM_BUFFER_SIZE * 2) / 2;
    int to_write = (n_samples < free_space) ? n_samples : free_space;
    
    for (int i = 0; i < to_write * 2; i++) {
        ctx->rx_modem_buffer[ctx->rx_modem_buffer_write_idx] = iq_samples[i];
        ctx->rx_modem_buffer_write_idx = (ctx->rx_modem_buffer_write_idx + 1) % (RADAE_MODEM_BUFFER_SIZE * 2);
    }
    
    pthread_cond_signal(&ctx->rx_cond);
    pthread_mutex_unlock(&ctx->rx_mutex);
    
    return to_write;
}

int radae_rx_read_speech(radae_context *ctx, float *samples, int max_samples)
{
    if (!ctx->rx_running || !samples || max_samples <= 0)
        return 0;
    
    pthread_mutex_lock(&ctx->rx_mutex);
    
    int available = BUFFER_SIZE(ctx->rx_speech_buffer_write_idx, 
                                 ctx->rx_speech_buffer_read_idx, 
                                 RADAE_SPEECH_BUFFER_SIZE);
    int to_read = (max_samples < available) ? max_samples : available;
    
    for (int i = 0; i < to_read; i++) {
        samples[i] = ctx->rx_speech_buffer[ctx->rx_speech_buffer_read_idx];
        ctx->rx_speech_buffer_read_idx = (ctx->rx_speech_buffer_read_idx + 1) % RADAE_SPEECH_BUFFER_SIZE;
    }
    
    pthread_mutex_unlock(&ctx->rx_mutex);
    
    return to_read;
}

// TX thread implementation
// Pipeline: speech (16kHz) -> lpcnet_demo -features -> inference.py -> modem IQ (8kHz)
static void *radae_tx_thread(void *arg)
{
    radae_context *ctx = (radae_context *)arg;
    
    // Build command for TX pipeline
    // RADEv2: lpcnet_demo -features -> radae_txe.py -> IQ samples
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "cd %s && "
             "build/src/lpcnet_demo -features - - | "
             "python3 radae_txe.py --model_name %s 2>/dev/null",
             ctx->radae_dir,
             RADAE_MODEL_PATH);
    
    fprintf(stderr, "RADAE TX: Starting pipeline: %s\n", cmd);
    
    // Create pipes for stdin and stdout
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        fprintf(stderr, "RADAE TX: Failed to create stdin pipe\n");
        ctx->tx_running = false;
        return NULL;
    }
    if (pipe(stdout_pipe) < 0) {
        fprintf(stderr, "RADAE TX: Failed to create stdout pipe\n");
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        ctx->tx_running = false;
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "RADAE TX: Fork failed\n");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        ctx->tx_running = false;
        return NULL;
    }
    
    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);  // Close write end of stdin pipe
        close(stdout_pipe[0]); // Close read end of stdout pipe
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
    
    // Parent process
    close(stdin_pipe[0]);  // Close read end of stdin pipe
    close(stdout_pipe[1]); // Close write end of stdout pipe
    
    ctx->tx_encoder_pid = pid;
    
    int write_fd = stdin_pipe[1];
    int read_fd = stdout_pipe[0];
    
    // Set non-blocking on read
    int flags = fcntl(read_fd, F_GETFL, 0);
    fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Buffer for reading speech and writing to pipe (16-bit PCM)
    int16_t pcm_buffer[RADAE_FRAME_SIZE];
    float speech_buffer[RADAE_FRAME_SIZE];
    
    // Buffer for reading IQ from pipe
    float iq_buffer[4096];
    
    while (ctx->tx_running && !ctx->shutdown_requested) {
        // Check for speech data in buffer
        pthread_mutex_lock(&ctx->tx_mutex);
        int available = BUFFER_SIZE(ctx->tx_speech_buffer_write_idx, 
                                     ctx->tx_speech_buffer_read_idx, 
                                     RADAE_SPEECH_BUFFER_SIZE);
        
        if (available < RADAE_FRAME_SIZE) {
            // Wait for more data
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 10000000; // 10ms timeout
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&ctx->tx_cond, &ctx->tx_mutex, &ts);
            pthread_mutex_unlock(&ctx->tx_mutex);
            
            // Still try to read output even if no input
            goto read_output;
        }
        
        // Read speech samples from buffer
        for (int i = 0; i < RADAE_FRAME_SIZE; i++) {
            speech_buffer[i] = ctx->tx_speech_buffer[ctx->tx_speech_buffer_read_idx];
            ctx->tx_speech_buffer_read_idx = (ctx->tx_speech_buffer_read_idx + 1) % RADAE_SPEECH_BUFFER_SIZE;
        }
        pthread_mutex_unlock(&ctx->tx_mutex);
        
        // Convert to 16-bit PCM for lpcnet_demo
        for (int i = 0; i < RADAE_FRAME_SIZE; i++) {
            float s = speech_buffer[i] * 32767.0f;
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            pcm_buffer[i] = (int16_t)s;
        }
        
        // Write to pipeline
        ssize_t written = write(write_fd, pcm_buffer, RADAE_FRAME_SIZE * sizeof(int16_t));
        if (written < 0 && errno != EAGAIN) {
            fprintf(stderr, "RADAE TX: Write error: %s\n", strerror(errno));
            break;
        }
        
    read_output:
        // Try to read modem IQ output
        ssize_t bytes_read = read(read_fd, iq_buffer, sizeof(iq_buffer));
        if (bytes_read > 0) {
            int n_floats = bytes_read / sizeof(float);
            
            pthread_mutex_lock(&ctx->tx_mutex);
            int free_space = BUFFER_FREE(ctx->tx_modem_buffer_write_idx, 
                                          ctx->tx_modem_buffer_read_idx, 
                                          RADAE_MODEM_BUFFER_SIZE * 2);
            int to_write = (n_floats < free_space) ? n_floats : free_space;
            
            for (int i = 0; i < to_write; i++) {
                ctx->tx_modem_buffer[ctx->tx_modem_buffer_write_idx] = iq_buffer[i];
                ctx->tx_modem_buffer_write_idx = (ctx->tx_modem_buffer_write_idx + 1) % (RADAE_MODEM_BUFFER_SIZE * 2);
            }
            pthread_mutex_unlock(&ctx->tx_mutex);
        }
    }
    
    close(write_fd);
    close(read_fd);
    
    // Wait for child process
    waitpid(pid, NULL, 0);
    ctx->tx_encoder_pid = 0;
    
    fprintf(stderr, "RADAE TX: Thread exiting\n");
    return NULL;
}

// RX thread implementation
// Pipeline: modem IQ (8kHz) -> rx2.py -> lpcnet_demo -fargan-synthesis -> speech (16kHz)
static void *radae_rx_thread(void *arg)
{
    radae_context *ctx = (radae_context *)arg;
    
    // RADEv2 RX pipeline: IQ samples -> radae_rxe.py -> features -> lpcnet_demo -fargan-synthesis
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "cd %s && "
             "python3 radae_rxe.py --model_name %s -v 0 | "
             "build/src/lpcnet_demo -fargan-synthesis - - 2>/dev/null",
             ctx->radae_dir,
             RADAE_MODEL_PATH);
    
    fprintf(stderr, "RADAE RX: Starting pipeline: %s\n", cmd);
    
    // Create pipes for stdin and stdout
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        fprintf(stderr, "RADAE RX: Failed to create stdin pipe\n");
        ctx->rx_running = false;
        return NULL;
    }
    if (pipe(stdout_pipe) < 0) {
        fprintf(stderr, "RADAE RX: Failed to create stdout pipe\n");
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        ctx->rx_running = false;
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "RADAE RX: Fork failed\n");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        ctx->rx_running = false;
        return NULL;
    }
    
    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
    
    // Parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    
    ctx->rx_decoder_pid = pid;
    
    int write_fd = stdin_pipe[1];
    int read_fd = stdout_pipe[0];
    
    // Set non-blocking on read
    int flags = fcntl(read_fd, F_GETFL, 0);
    fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);
    
    float iq_buffer[4096];
    int16_t pcm_buffer[2048];
    
    while (ctx->rx_running && !ctx->shutdown_requested) {
        // Check for modem IQ data in buffer
        pthread_mutex_lock(&ctx->rx_mutex);
        int available = BUFFER_SIZE(ctx->rx_modem_buffer_write_idx, 
                                     ctx->rx_modem_buffer_read_idx, 
                                     RADAE_MODEM_BUFFER_SIZE * 2);
        
        if (available < 2) {
            // Wait for more data
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 10000000; // 10ms timeout
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&ctx->rx_cond, &ctx->rx_mutex, &ts);
            pthread_mutex_unlock(&ctx->rx_mutex);
            
            goto read_output_rx;
        }
        
        // Read up to 4096 floats from buffer
        int to_read = (available < 4096) ? available : 4096;
        for (int i = 0; i < to_read; i++) {
            iq_buffer[i] = ctx->rx_modem_buffer[ctx->rx_modem_buffer_read_idx];
            ctx->rx_modem_buffer_read_idx = (ctx->rx_modem_buffer_read_idx + 1) % (RADAE_MODEM_BUFFER_SIZE * 2);
        }
        pthread_mutex_unlock(&ctx->rx_mutex);
        
        // Write to pipeline
        ssize_t written = write(write_fd, iq_buffer, to_read * sizeof(float));
        if (written < 0 && errno != EAGAIN) {
            fprintf(stderr, "RADAE RX: Write error: %s\n", strerror(errno));
            break;
        }
        
    read_output_rx:
        // Try to read speech output (16-bit PCM)
        ssize_t bytes_read = read(read_fd, pcm_buffer, sizeof(pcm_buffer));
        if (bytes_read > 0) {
            int n_samples = bytes_read / sizeof(int16_t);
            
            pthread_mutex_lock(&ctx->rx_mutex);
            int free_space = BUFFER_FREE(ctx->rx_speech_buffer_write_idx, 
                                          ctx->rx_speech_buffer_read_idx, 
                                          RADAE_SPEECH_BUFFER_SIZE);
            int to_write = (n_samples < free_space) ? n_samples : free_space;
            
            for (int i = 0; i < to_write; i++) {
                ctx->rx_speech_buffer[ctx->rx_speech_buffer_write_idx] = (float)pcm_buffer[i] / 32768.0f;
                ctx->rx_speech_buffer_write_idx = (ctx->rx_speech_buffer_write_idx + 1) % RADAE_SPEECH_BUFFER_SIZE;
            }
            pthread_mutex_unlock(&ctx->rx_mutex);
        }
    }
    
    close(write_fd);
    close(read_fd);
    
    waitpid(pid, NULL, 0);
    ctx->rx_decoder_pid = 0;
    
    fprintf(stderr, "RADAE RX: Thread exiting\n");
    return NULL;
}

// Sample rate conversion utilities

// Simple linear interpolation resampler
static void resample_linear(const double *in, int in_len, double *out, int out_len)
{
    if (in_len <= 0 || out_len <= 0) return;
    
    double ratio = (double)(in_len - 1) / (double)(out_len - 1);
    
    for (int i = 0; i < out_len; i++) {
        double pos = i * ratio;
        int idx = (int)pos;
        double frac = pos - idx;
        
        if (idx >= in_len - 1) {
            out[i] = in[in_len - 1];
        } else {
            out[i] = in[idx] * (1.0 - frac) + in[idx + 1] * frac;
        }
    }
}

void resample_96k_to_16k(const double *in, int in_len, float *out, int *out_len)
{
    // 96kHz to 16kHz = 6:1 decimation
    *out_len = in_len / 6;
    double temp[*out_len];
    resample_linear(in, in_len, temp, *out_len);
    for (int i = 0; i < *out_len; i++) {
        out[i] = (float)temp[i];
    }
}

void resample_16k_to_96k(const float *in, int in_len, double *out, int *out_len)
{
    // 16kHz to 96kHz = 1:6 interpolation
    *out_len = in_len * 6;
    double temp[in_len];
    for (int i = 0; i < in_len; i++) {
        temp[i] = (double)in[i];
    }
    resample_linear(temp, in_len, out, *out_len);
}

void resample_48k_to_16k(const double *in, int in_len, float *out, int *out_len)
{
    // 48kHz to 16kHz = 3:1 decimation
    *out_len = in_len / 3;
    double temp[*out_len];
    resample_linear(in, in_len, temp, *out_len);
    for (int i = 0; i < *out_len; i++) {
        out[i] = (float)temp[i];
    }
}

void resample_96k_to_8k(const double *in, int in_len, float *out, int *out_len)
{
    // 96kHz to 8kHz = 12:1 decimation
    *out_len = in_len / 12;
    double temp[*out_len];
    resample_linear(in, in_len, temp, *out_len);
    for (int i = 0; i < *out_len; i++) {
        out[i] = (float)temp[i];
    }
}

void resample_8k_to_96k(const float *in, int in_len, double *out, int *out_len)
{
    // 8kHz to 96kHz = 1:12 interpolation
    *out_len = in_len * 12;
    double temp[in_len];
    for (int i = 0; i < in_len; i++) {
        temp[i] = (double)in[i];
    }
    resample_linear(temp, in_len, out, *out_len);
}
