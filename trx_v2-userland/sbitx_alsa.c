/* sBitx controller
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

#include "sbitx_alsa.h"


void show_alsa(snd_pcm_t *handle, snd_pcm_hw_params_t *params)
{
    unsigned int val, val2;
    int dir = 0;
    snd_pcm_uframes_t frames;

    /* Display information about the PCM interface */

    printf("PCM handle name = '%s'\n",
           snd_pcm_name(handle));

    printf("PCM state = %s\n",
           snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_access(params,
                                 (snd_pcm_access_t *) &val);
    printf("access type = %s\n",
           snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *)&val);
    printf("format = '%s' (%s)\n",
           snd_pcm_format_name((snd_pcm_format_t)val),
           snd_pcm_format_description(
               (snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(params,
                                    (snd_pcm_subformat_t *)&val);
    printf("subformat = '%s' (%s)\n",
           snd_pcm_subformat_name((snd_pcm_subformat_t)val),
           snd_pcm_subformat_description(
               (snd_pcm_subformat_t)val));

    snd_pcm_hw_params_get_channels(params, &val);
    printf("channels = %d\n", val);

    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("rate = %d bps\n", val);

    snd_pcm_hw_params_get_periods(params, &val, &dir);
    printf("periods per buffer = %d frames\n", val);

    snd_pcm_hw_params_get_period_size(params,
                                    &frames, &dir);
    printf("period size = %d frames\n", (int)frames);

    snd_pcm_hw_params_get_buffer_size(params,
                                      (snd_pcm_uframes_t *) &val);
    printf("buffer size = %d frames\n", val);


    snd_pcm_hw_params_get_period_time(params,
                                    &val, &dir);
    printf("period time = %d us\n", val);

    snd_pcm_hw_params_get_buffer_time(params,
                                    &val, &dir);
    printf("buffer time = %d us\n", val);

    snd_pcm_hw_params_get_rate_numden(params,
                                      &val, &val2);
    printf("exact rate = %d/%d bps\n", val, val2);

    val = snd_pcm_hw_params_get_sbits(params);
    printf("significant bits = %d\n", val);

    snd_pcm_hw_params_get_tick_time(params,
                                    &val, &dir);
    printf("tick time = %d us\n", val);

    val = snd_pcm_hw_params_is_batch(params);
    printf("is batch double buffering = %d\n", val);

    val = snd_pcm_hw_params_is_block_transfer(params);
    printf("is block transfer = %d\n", val);

    val = snd_pcm_hw_params_is_double(params);
    printf("is double buffered = %d\n", val);

    val = snd_pcm_hw_params_is_half_duplex(params);
    printf("is half duplex = %d\n", val);

    val = snd_pcm_hw_params_is_joint_duplex(params);
    printf("is joint duplex = %d\n", val);

    val = snd_pcm_hw_params_can_overrange(params);
    printf("can overrange = %d\n", val);

    val = snd_pcm_hw_params_can_mmap_sample_resolution(params);
    printf("can mmap = %d\n", val);

    val = snd_pcm_hw_params_can_pause(params);
    printf("can pause = %d\n", val);

    val = snd_pcm_hw_params_can_resume(params);
    printf("can resume = %d\n", val);

    val = snd_pcm_hw_params_can_sync_start(params);
    printf("can sync start = %d\n", val);

    val = snd_pcm_hw_params_get_fifo_size(params);
    printf("fifo size = %d\n", val);

    val = snd_pcm_hw_params_is_monotonic(params);
    printf("is monotonic = %d\n", val);

}


void *radio_capture_thread()
{

    return NULL;
}


void *radio_playback_thread()
{


    return NULL;
}

void *loop_capture_thread()
{


    return NULL;
}

void *loop_playback_thread()
{


    return NULL;
}


void sound_system()
{
    // here we start the sound system and create all the threads

}
