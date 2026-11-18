/* capture_levels - the level of each channel the codec captures, for the log.
 *
 * Copyright (C) 2026 Rhizomatica
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The sBitx codec captures two channels: the receiver on the left, the
 * microphone on the right.  A receiver that goes deaf after a daemon start
 * (no signal, the noise still there) can be one of two things: the codec's
 * line input left powered down, or left and right swapped at the start of
 * the capture.  These figures, logged at each stream start and every
 * minute, tell them apart.  On two healthy sBitx with no microphone the
 * receiver reads about -70 dBFS rms (HF ratio about 0.3) and the
 * microphone about -57 dBFS (HF ratio 0.4-0.5): a swap shows as each
 * channel taking the other's figures, a powered-down input as the left
 * channel falling to the codec's own floor.
 */
#ifndef CAPTURE_LEVELS_H_
#define CAPTURE_LEVELS_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define CAPTURE_LEVELS_CHANNELS 2

/* The HF ratio is rms(x[n] - x[n-1]) / rms(x): 2 sin(pi f / fs) for a tone
 * at f.  At 96 kHz it is 1.41 at 24 kHz and 0.07 at 1 kHz, so it says where
 * a channel's energy is without a transform.
 *
 * The codec runs with its ADC high-pass filter off, so each channel carries
 * a DC offset that can be larger than its signal: the levels are of what is
 * left once the mean is taken out, and the offset is reported on its own.
 * Sums are kept relative to each channel's first sample, which keeps the
 * mean's removal exact in doubles over minutes of samples. */
typedef struct
{
    double   sum[CAPTURE_LEVELS_CHANNELS];
    double   sum_sq[CAPTURE_LEVELS_CHANNELS];
    double   diff_sq[CAPTURE_LEVELS_CHANNELS];
    int32_t  ref[CAPTURE_LEVELS_CHANNELS];
    int32_t  prev[CAPTURE_LEVELS_CHANNELS];
    int32_t  min[CAPTURE_LEVELS_CHANNELS];
    int32_t  max[CAPTURE_LEVELS_CHANNELS];
    uint64_t frames;
} capture_levels;

typedef struct
{
    double rms_dbfs;     /* the signal without its DC; -INFINITY when constant */
    double peak_dbfs;    /* half the peak-to-peak swing */
    double dc_dbfs;      /* the mean's magnitude; -INFINITY for none */
    double hf_ratio;     /* 0 when constant */
} capture_channel_level;

void capture_levels_reset(capture_levels *c);

/* Interleaved stereo S32 frames, as the codec delivers them. */
void capture_levels_add(capture_levels *c, const int32_t *frames, size_t n_frames);

void capture_levels_get(const capture_levels *c, int channel, capture_channel_level *out);

#endif
