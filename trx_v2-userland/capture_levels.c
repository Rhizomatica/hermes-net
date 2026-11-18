/* capture_levels - see capture_levels.h.
 *
 * Copyright (C) 2026 Rhizomatica
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <math.h>
#include <string.h>

#include "capture_levels.h"

/* Full scale of an S32 sample. */
#define CAPTURE_FULL_SCALE 2147483648.0

void capture_levels_reset(capture_levels *c)
{
    memset(c, 0, sizeof(*c));
}

void capture_levels_add(capture_levels *c, const int32_t *frames, size_t n_frames)
{
    for (size_t i = 0; i < n_frames; i++)
    {
        bool first = c->frames == 0 && i == 0;
        for (int ch = 0; ch < CAPTURE_LEVELS_CHANNELS; ch++)
        {
            int32_t v = frames[i * CAPTURE_LEVELS_CHANNELS + ch];
            if (first)
            {
                c->ref[ch] = v;
                c->min[ch] = c->max[ch] = v;
            }
            else
            {
                double d = (double) v - (double) c->prev[ch];
                c->diff_sq[ch] += d * d;
            }
            double x = (double) v - (double) c->ref[ch];
            c->sum[ch] += x;
            c->sum_sq[ch] += x * x;
            if (v < c->min[ch]) c->min[ch] = v;
            if (v > c->max[ch]) c->max[ch] = v;
            c->prev[ch] = v;
        }
    }
    c->frames += n_frames;
}

static double dbfs(double magnitude)
{
    return magnitude > 0.0 ? 20.0 * log10(magnitude / CAPTURE_FULL_SCALE) : -INFINITY;
}

void capture_levels_get(const capture_levels *c, int channel, capture_channel_level *out)
{
    out->rms_dbfs = out->peak_dbfs = out->dc_dbfs = -INFINITY;
    out->hf_ratio = 0.0;
    if (channel < 0 || channel >= CAPTURE_LEVELS_CHANNELS || c->frames == 0)
        return;
    double n = (double) c->frames;
    double mean = c->sum[channel] / n;
    double var = c->sum_sq[channel] / n - mean * mean;
    out->dc_dbfs = dbfs(fabs((double) c->ref[channel] + mean));
    out->peak_dbfs = dbfs(((double) c->max[channel] - (double) c->min[channel]) / 2.0);
    if (var <= 0.0)
        return;
    out->rms_dbfs = dbfs(sqrt(var));
    if (c->frames > 1)
        out->hf_ratio = sqrt((c->diff_sq[channel] / (n - 1.0)) / var);
}
