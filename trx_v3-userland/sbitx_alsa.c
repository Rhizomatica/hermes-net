/* sBitx controller
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

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "sbitx_alsa.h"
#include "sbitx_dsp.h"
#include "sbitx_buffer.h"

char *radio_capture_dev = "hw:0,0";
char *radio_playback_dev = "hw:0,0";
char *loop_capture_dev = "hw:2,1";
char *loop_playback_dev = "hw:1,0";

snd_pcm_t *pcm_capture_handle;
snd_pcm_t *pcm_play_handle;
snd_pcm_t *loopback_capture_handle;
snd_pcm_t *loopback_play_handle;

unsigned int hw_rate = 96000; /* Sample rate */
snd_pcm_uframes_t hw_period_size = 512; // in frames
uint64_t hw_n_periods = 4; // number of periods

unsigned int loopback_rate = 48000; /* Sample rate */
snd_pcm_uframes_t loopback_period_size = 256; // in frames
uint64_t loopback_n_periods = 4; // number of periods

snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
uint32_t channels = 2;

// local radio handle used to easy parameter passing
static radio *radio_h_snd;
extern _Atomic bool shutdown_;


void show_alsa(snd_pcm_t *handle, snd_pcm_hw_params_t *params)
{
    unsigned int val, val2;
    int dir = 0;
    snd_pcm_uframes_t frames;

    /* Display information about the PCM interface */

    printf("PCM handle name = '%s'\n", snd_pcm_name(handle));

    printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
    printf("access type = %s\n",snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *)&val);
    printf("format = '%s' (%s)\n",
           snd_pcm_format_name((snd_pcm_format_t)val),
           snd_pcm_format_description(
               (snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(params, (snd_pcm_subformat_t *)&val);
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

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    printf("period size = %d frames\n", (int)frames);

    snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);
    printf("buffer size = %d frames\n", val);


    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    printf("period time = %d us\n", val);

    snd_pcm_hw_params_get_buffer_time(params, &val, &dir);
    printf("buffer time = %d us\n", val);

    snd_pcm_hw_params_get_rate_numden(params, &val, &val2);
    printf("exact rate = %d/%d bps\n", val, val2);

    val = snd_pcm_hw_params_get_sbits(params);
    printf("significant bits = %d\n", val);

    snd_pcm_hw_params_get_tick_time(params, &val, &dir);
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

// TODO: rewrite
void setup_audio_codec()
{

	//configure all the channels of the mixer
	sound_mixer(radio_capture_dev, "Input Mux", 0);
	sound_mixer(radio_capture_dev, "Line", 1);
	sound_mixer(radio_capture_dev, "Mic", 0);
	sound_mixer(radio_capture_dev, "Mic Boost", 0);
	sound_mixer(radio_capture_dev, "Playback Deemphasis", 0);

	sound_mixer(radio_playback_dev, "Master", 10);
	sound_mixer(radio_playback_dev, "Output Mixer HiFi", 1);
	sound_mixer(radio_playback_dev, "Output Mixer Mic Sidetone", 0);

}

void sound_mixer(char *card_name, char *element, int make_on)
{
    // this is alsa-less operation...
    if (radio_h_snd->profiles[radio_h_snd->profile_active_idx].operating_mode == OPERATING_MODE_CONTROLS_ONLY)
        return;

    long min, max;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    char *card = card_name;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, element);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    //find out if the his element is capture side or plaback
    if(snd_mixer_selem_has_capture_switch(elem))
    {
        snd_mixer_selem_set_capture_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_switch(elem))
    {
        snd_mixer_selem_set_playback_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_volume(elem))
    {
        long volume = make_on;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
    }
    else if (snd_mixer_selem_has_capture_volume(elem))
    {
        long volume = make_on;
        snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
        snd_mixer_selem_set_capture_volume_all(elem, volume * max / 100);
    }
    else if (snd_mixer_selem_is_enumerated(elem))
    {
        snd_mixer_selem_set_enum_item(elem, 0, make_on);
    }
    snd_mixer_close(handle);
}



void *radio_capture_thread(void *device_ptr)
{
    char *device = (char *) device_ptr;
    uint32_t exact_rate;

    int e;
    snd_pcm_hw_params_t *hwparams;

    if ((e = snd_pcm_hw_params_malloc (&hwparams)) < 0)
    {
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n", snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&pcm_capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n", device, snd_strerror (e));
        return NULL;
    }

    fprintf(stderr, "ALSA Capture device is: %s\n", device);

    if ((e = snd_pcm_hw_params_any(pcm_capture_handle, hwparams)) < 0)
    {
        fprintf(stderr, "Error setting capture access (%d)\n", e);
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_access(pcm_capture_handle, hwparams, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Error setting capture access.\n");
        return NULL;
    }

    e = snd_pcm_hw_params_set_format(pcm_capture_handle, hwparams, format);
    if (e < 0)
    {
        fprintf(stderr, "Error setting capture format.\n");
        return NULL;
    }


    exact_rate = hw_rate;
    e = snd_pcm_hw_params_set_rate_near(pcm_capture_handle, hwparams, &exact_rate, 0);
    if ( e < 0)
    {
        fprintf(stderr, "Error setting capture rate.\n");
        return NULL;
    }

    if (hw_rate != exact_rate)
        fprintf(stderr, "Capture rate %d changed to %d Hz\n", hw_rate, exact_rate);

    if ((e = snd_pcm_hw_params_set_channels(pcm_capture_handle, hwparams, channels)) < 0)
    {
        fprintf(stderr, "Error setting capture channels.\n");
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_period_size_near(pcm_capture_handle,hwparams, &hw_period_size, 0)) < 0)
    {
        fprintf (stderr, "Can not set period size (%s)\n",snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_periods(pcm_capture_handle, hwparams, hw_n_periods, 0)) < 0)
    {
        fprintf(stderr, "Error setting capture periods.\n");
        return NULL;
    }

    if ((e = snd_pcm_hw_params(pcm_capture_handle, hwparams)) < 0)
    {
        fprintf(stderr, "Error setting capture HW params.\n");
        return NULL;
    }

    printf("============= REPORT RADIO CAPTURE DEVICE %s =================\n", device);
    show_alsa(pcm_capture_handle, hwparams);
    printf("==============================================================\n");

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = hw_period_size * sample_size * channels;

    uint8_t *buffer = malloc(buffer_size);
    uint8_t *radio = (uint8_t *) malloc(buffer_size/2);
    uint8_t *mic = (uint8_t *) malloc(buffer_size/2);

    snd_pcm_prepare(pcm_capture_handle);
    snd_pcm_drop(pcm_capture_handle);
    snd_pcm_prepare(pcm_capture_handle);

    while (!shutdown_)
    {

        if ((e = snd_pcm_mmap_readi(pcm_capture_handle, buffer, hw_period_size)) != hw_period_size)
        {
            fprintf (stderr, "read from audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stderr, "overrun\n");
            }
            else if (e < 0)
            {
                fprintf(stderr, "error from readi: %s\n", snd_strerror(e));
            } else if (e != hw_period_size)
            {
                fprintf(stderr, "short read, read %d frames\n", e);
            }
            snd_pcm_prepare (pcm_capture_handle);
            continue;
        }

        for (int j = 0; j < hw_period_size; j++)
        {
            memcpy(&radio[j*sample_size], &buffer[j * sample_size * channels], sample_size);
            // attenuate the mic
            int32_t *sample = (int32_t *) &buffer[j * sample_size * channels + sample_size];
            *sample /= 15;
            memcpy(&mic[j*sample_size], sample, sample_size);
//            memcpy(&mic[j*sample_size], &buffer[j * sample_size * channels + sample_size], sample_size); // this would be just the copy if we did not need to change the sample

        }

        write_buffer(radio_to_dsp, radio, buffer_size/2);
        write_buffer(mic_to_dsp, mic, buffer_size/2);

        // write to buffers
    }

    snd_pcm_hw_params_free(hwparams);
    free(buffer);
    free(radio);
    free(mic);

    return NULL;
}


void *radio_playback_thread(void *device_ptr)
{
    char *device = (char *) device_ptr;
    uint32_t exact_rate;

    int e;
    snd_pcm_hw_params_t *hwparams;

    if ((e = snd_pcm_hw_params_malloc (&hwparams)) < 0)
    {
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n", snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&pcm_play_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n", device, snd_strerror (e));
        return NULL;
    }


    fprintf(stderr, "ALSA Playback device is: %s\n", device);

    if ((e = snd_pcm_hw_params_any(pcm_play_handle, hwparams)) < 0)
    {
        fprintf(stderr, "Error getting hw playback params (%d)\n", e);
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_access(pcm_play_handle, hwparams, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Error setting playback access.\n");
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_format(pcm_play_handle, hwparams, format)) < 0)
    {
        fprintf(stderr, "Error setting playback format.\n");
        return NULL;
    }

    exact_rate = hw_rate;
    e = snd_pcm_hw_params_set_rate_near(pcm_play_handle, hwparams, &exact_rate, 0);
    if ( e < 0)
    {
        fprintf(stderr, "Error setting playback rate.\n");
        return NULL;
    }

    if (hw_rate != exact_rate)
        fprintf(stderr, "Playback rate %d changed to %d Hz\n", hw_rate, exact_rate);

        /* Set number of channels */
    if ((e = snd_pcm_hw_params_set_channels(pcm_play_handle, hwparams, channels)) < 0)
    {
        fprintf(stderr, "Error setting playback channels.\n");
        return NULL;
    }

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(pcm_play_handle,hwparams, &hw_period_size, 0)) < 0)
    {
        fprintf (stderr, "Can not set period size (%s)\n", snd_strerror(e));
        return NULL;
    }

    /* nr. of periods */
    if ((e = snd_pcm_hw_params_set_periods(pcm_play_handle, hwparams, hw_n_periods, 0)) < 0)
    {
        fprintf(stderr, "Error setting playback periods.\n");
        return NULL;
    }

    if ((e = snd_pcm_hw_params(pcm_play_handle, hwparams)) < 0)
    {
        fprintf(stderr, "Error setting playback HW params.\n");
        return NULL;
    }

    printf("============= REPORT RADIO PLAYBACK DEVICE %s ================\n", device);
    show_alsa(pcm_play_handle, hwparams);
    printf("==============================================================\n");

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = hw_period_size * sample_size * channels;

    uint8_t *buffer = malloc(buffer_size);
    uint8_t *radio = (uint8_t *) malloc(buffer_size/2);
    uint8_t *speaker = (uint8_t *) malloc(buffer_size/2);

    snd_pcm_prepare(pcm_play_handle);
    snd_pcm_drop(pcm_play_handle);
    snd_pcm_prepare(pcm_play_handle);

    while (!shutdown_)
    {

        read_buffer(dsp_to_radio, radio, buffer_size/2);
        read_buffer(dsp_to_speaker, speaker, buffer_size/2);

        for (int j = 0; j < hw_period_size; j++)
        {
            // int32_t *sample = (int32_t *) &speaker[j * sample_size]; *sample *= 100;
            memcpy(&buffer[j*sample_size*channels], &speaker[j*sample_size], sample_size);
            memcpy(&buffer[j*sample_size*channels + sample_size], &radio[j*sample_size], sample_size);
        }

    try_again_radio_play:
        if ((e = snd_pcm_mmap_writei(pcm_play_handle, buffer, hw_period_size)) != hw_period_size)
        {
            fprintf (stderr, "write to audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stderr, "overrun\n");
            }
            else if (e < 0)
            {
                fprintf(stderr, "error from writei: %s\n", snd_strerror(e));
            } else if (e != hw_period_size)
            {
                fprintf(stderr, "short write, wrote %d frames\n", e);
            }
            snd_pcm_prepare (pcm_play_handle);
            goto try_again_radio_play;
        }
    }

    snd_pcm_hw_params_free(hwparams);
    free(buffer);

    return NULL;
}

void *loop_capture_thread(void *device_ptr)
{
    char *device = (char *) device_ptr;
    uint32_t exact_rate;

    int e;
    snd_pcm_hw_params_t *hloop_params;

    if ((e = snd_pcm_hw_params_malloc (&hloop_params)) < 0)
    {
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n", snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&loopback_capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n", device, snd_strerror (e));
        return NULL;
    }

    fprintf(stderr, "ALSA Loopback Capture device at: %s\n", device);

    if ((e = snd_pcm_hw_params_any(loopback_capture_handle, hloop_params)) < 0)
    {
        fprintf(stderr, "Error setting capture access (%d)\n", e);
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_access(loopback_capture_handle, hloop_params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Error setting capture access.\n");
        return NULL;
    }

    /* Set sample format */
    if ((e = snd_pcm_hw_params_set_format(loopback_capture_handle, hloop_params, format)) < 0)
    {
        fprintf(stderr, "Error setting loopback capture format.\n");
        return NULL;
    }

    exact_rate = loopback_rate;
    if ((e = snd_pcm_hw_params_set_rate_near(loopback_capture_handle, hloop_params, &exact_rate, 0)) < 0)
    {
        fprintf(stderr, "Error setting loopback capture rate.\n");
        return NULL;
    }

    if (loopback_rate != exact_rate)
        fprintf(stderr, "The loopback capture rate set to %d Hz\n", exact_rate);

    /* Set number of channels */
    if ((e = snd_pcm_hw_params_set_channels(loopback_capture_handle, hloop_params, channels)) < 0)
    {
        fprintf(stderr, "Error setting loopback capture channels.\n");
        return NULL;
    }

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(loopback_capture_handle, hloop_params, &loopback_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_periods(loopback_capture_handle, hloop_params, loopback_n_periods, 0)) < 0) {
        fprintf(stderr, "*Error setting loopback capture periods.\n");
        return NULL;
    }


    if ((e = snd_pcm_hw_params(loopback_capture_handle, hloop_params)) < 0)
    {
        fprintf(stderr, "*Error setting capture HW params.\n");
        return NULL;
    }

    printf("============= REPORT LOOPBACK CAPTURE DEVICE %s ==============\n", device);
    show_alsa(loopback_capture_handle, hloop_params);
    printf("==============================================================\n");

    // apply sw parameters... ?

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = loopback_period_size * sample_size * channels;

    uint8_t *buffer = malloc(buffer_size);

    snd_pcm_prepare(loopback_capture_handle);
    snd_pcm_drop(loopback_capture_handle);
    snd_pcm_prepare(loopback_capture_handle);

    while (!shutdown_)
    {

        if ((e = snd_pcm_mmap_readi(loopback_capture_handle, buffer, loopback_period_size)) != loopback_period_size)
        {

            fprintf (stderr, "read from audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stderr, "overrun\n");
            }
            else if (e < 0) {
                fprintf(stderr,"error from readi: %s\n", snd_strerror(e));
            } else if (e != loopback_period_size)
            {
                fprintf(stderr, "short read, read %d frames\n", e);
            }
            snd_pcm_prepare (loopback_capture_handle);
            continue;
        }

        // attenuate 10db the loopback
        for (int i = 0; i < loopback_period_size; i++)
        {
            int32_t *sample1 = (int32_t *) &buffer[i * sample_size * channels];
            int32_t *sample2 = (int32_t *) &buffer[i * sample_size * channels + sample_size];
            *sample1 /= 10;
            *sample2 /= 10;
        }

        write_buffer(loopback_to_dsp, buffer, buffer_size);
    }

    snd_pcm_hw_params_free(hloop_params);
    free(buffer);

    return NULL;
}

void *loop_playback_thread(void *device_ptr)
{
    char *device = (char *) device_ptr;
    uint32_t exact_rate;

    int e;
    snd_pcm_hw_params_t *hloop_params;


    if ((e = snd_pcm_hw_params_malloc (&hloop_params)) < 0)
    {
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n", snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&loopback_play_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n", device, snd_strerror (e));
        return NULL;
    }

    fprintf(stderr, "ALSA Loopback Playback device at: %s\n", device);

    if ((e = snd_pcm_hw_params_any(loopback_play_handle, hloop_params)) < 0)
    {
        fprintf(stderr, "Error getting loopback playback params (%d)\n", e);
        return NULL;
    }

    if ((e = snd_pcm_hw_params_set_access(loopback_play_handle, hloop_params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Error setting loopback access.\n");
        return NULL;
    }

    /* Set sample format */
    if ((e = snd_pcm_hw_params_set_format(loopback_play_handle, hloop_params, format)) < 0)
    {
        fprintf(stderr, "Error setting loopback format.\n");
        return NULL;
    }

    exact_rate = loopback_rate;
    if ((e = snd_pcm_hw_params_set_rate_near(loopback_play_handle, hloop_params, &exact_rate, 0)) < 0)
    {
        fprintf(stderr, "Error setting playback rate.\n");
        return NULL;
    }

    if (loopback_rate != exact_rate)
        fprintf(stderr, "Loopback playback rate %d changed to %d Hz\n", loopback_rate, exact_rate);


    /* Set number of channels */
    if ((e = snd_pcm_hw_params_set_channels(loopback_play_handle, hloop_params, channels)) < 0)
    {
        fprintf(stderr, "*Error setting playback channels.\n");
        return NULL;
    }

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(loopback_play_handle, hloop_params, &loopback_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return NULL;
    }


    if ((e = snd_pcm_hw_params_set_periods(loopback_play_handle, hloop_params, loopback_n_periods, 0)) < 0)
    {
        fprintf(stderr, "*Error setting playback periods.\n");
        return NULL;
    }

    if ((e = snd_pcm_hw_params(loopback_play_handle, hloop_params)) < 0)
    {
        fprintf(stderr, "*Error setting loopback playback HW params.\n");
        return NULL;
    }

    printf("============= REPORT LOOPBACK PLAYBACK DEVICE %s =============\n", device);
    show_alsa(loopback_play_handle, hloop_params);
    printf("==============================================================\n");

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = loopback_period_size * sample_size * channels;

    uint8_t *buffer = malloc(buffer_size);

    snd_pcm_prepare(loopback_play_handle);
    snd_pcm_drop(loopback_play_handle);
    snd_pcm_prepare(loopback_play_handle);

    while (!shutdown_)
    {
        read_buffer(dsp_to_loopback, buffer, buffer_size);

    try_again_loop_play:
        if ((e = snd_pcm_mmap_writei(loopback_play_handle, buffer, loopback_period_size)) != loopback_period_size)
        {
            fprintf (stderr, "write to audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stderr, "overrun\n");
            }
            else if (e < 0)
            {
                fprintf(stderr, "error from writei: %s\n", snd_strerror(e));
            } else if (e != loopback_period_size)
            {
                fprintf(stderr, "short write, wrote %d frames\n", e);
            }

            snd_pcm_prepare (loopback_play_handle);
            goto try_again_loop_play;
        }
    }

    snd_pcm_hw_params_free(hloop_params);
    free(buffer);

    return NULL;
}

void *control_thread(void *device_ptr)
{
    int sample_size = snd_pcm_format_width(format) / 8;

    // TODO: DSP with 512 sample window?
    // we have 96 kHz in the radio soundcard, and 48 kHz in the loopback soundcard
    // we define our block transfer size as the minimum of both, in order to try to reduce latency a bit
    // uint32_t block_size = hw_period_size;
    // As our DSP code needs 1024 samples window to work... we force 1024
    uint32_t block_size = 1024;

    // as we are hardcoding block sizes... this gets false
#if 0
    if (hw_period_size != (loopback_period_size * 2))
    {
        fprintf(stderr, "Hardware 96 kHz sound period size != (Loopback 48 kHz period size * 2)\n");
        block_size = hw_period_size;
    }
#endif

    uint32_t buffer_size = block_size * sample_size;

    uint8_t *buffer_radio_to_dsp = malloc(buffer_size);
    uint8_t *buffer_mic_to_dsp = malloc(buffer_size);
    uint8_t *buffer_loop_to_dsp = malloc(buffer_size);

    uint8_t *signal_to_tx;
    uint8_t *output_speaker; uint8_t *output_loopback; uint8_t *output_tx;

    output_tx = malloc(buffer_size);
    output_speaker = malloc(buffer_size);
    output_loopback = malloc(buffer_size);

    while (!shutdown_)
    {
        _Atomic bool use_loopback = (radio_h_snd->profiles[radio_h_snd->profile_active_idx].operating_mode == OPERATING_MODE_FULL_LOOPBACK) ? true : false;

        read_buffer(radio_to_dsp, buffer_radio_to_dsp, buffer_size); // mono
        read_buffer(mic_to_dsp, buffer_mic_to_dsp, buffer_size); // mono

        if (use_loopback)
        {
            read_buffer(loopback_to_dsp, buffer_loop_to_dsp, buffer_size); // stereo interleaved
            signal_to_tx = buffer_loop_to_dsp;
        }
        else
        {
            clear_buffer(loopback_to_dsp);
            signal_to_tx = buffer_mic_to_dsp;
        }

        if (radio_h_snd->txrx_state == IN_RX)
        {
            dsp_process_rx(buffer_radio_to_dsp, output_speaker, output_loopback, output_tx, block_size);
        }
        else
        {
            dsp_process_tx(signal_to_tx, output_speaker, output_loopback, output_tx, block_size, use_loopback);
        }

        write_buffer(dsp_to_loopback, output_loopback, buffer_size); // stereo 48 kHz interleaved
        write_buffer(dsp_to_radio, output_tx, buffer_size); // mono 96 kHz
        write_buffer(dsp_to_speaker, output_speaker, buffer_size); // mono 96 kHz
    }

    free(output_tx);
    free(output_speaker);
    free(output_loopback);

    return NULL;
}


void clear_buffers()
{
    clear_buffer(radio_to_dsp);
    clear_buffer(dsp_to_radio);
    clear_buffer(mic_to_dsp);
    clear_buffer(dsp_to_speaker);
    clear_buffer(dsp_to_loopback);
    clear_buffer(loopback_to_dsp);
}

// initialize the ALSA sound system
void sound_system_init(radio *radio_h, pthread_t *control_tid, pthread_t *radio_capture,
                       pthread_t *radio_playback, pthread_t *loop_capture, pthread_t *loop_playback)
{
    radio_h_snd = radio_h;

    initialize_buffers();

    pthread_create(radio_playback, NULL, radio_playback_thread, (void*)radio_playback_dev);
    pthread_create(loop_playback, NULL, loop_playback_thread, (void*)loop_playback_dev);

    pthread_create(control_tid, NULL, control_thread, NULL);

    pthread_create(radio_capture, NULL, radio_capture_thread, (void*)radio_capture_dev);
    pthread_create(loop_capture, NULL, loop_capture_thread, (void*)loop_capture_dev);


    struct sched_param sch;
    sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(*radio_capture, SCHED_FIFO, &sch);
    pthread_setschedparam(*radio_playback, SCHED_FIFO, &sch);
    pthread_setschedparam(*loop_capture, SCHED_FIFO, &sch);
    pthread_setschedparam(*loop_playback, SCHED_FIFO, &sch);

}

// shutdown the ALSA sound system
void sound_system_shutdown(radio *radio_h, pthread_t *control_tid, pthread_t *radio_capture,
                           pthread_t *radio_playback, pthread_t *loop_capture, pthread_t *loop_playback)
{
    pthread_join(*radio_playback, NULL);
    pthread_join(*loop_playback, NULL);

    pthread_join(*control_tid, NULL);

    pthread_join(*radio_capture, NULL);
    pthread_join(*loop_capture, NULL);
}
