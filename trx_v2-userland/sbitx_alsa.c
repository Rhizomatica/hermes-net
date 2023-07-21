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

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "sbitx_alsa.h"
#include "buffer.h"

char *radio_capture_dev = "hw:0,0";
char *radio_playback_dev = "hw:0,0";
char *loop_capture_dev = "hw:2,1";
char *loop_playback_dev = "hw:1,0";

snd_pcm_t *pcm_capture_handle;
snd_pcm_t *pcm_play_handle;
snd_pcm_t *loopback_capture_handle;
snd_pcm_t *loopback_play_handle;

pthread_mutex_t radio_capture_mutex;
pthread_mutex_t radio_play_mutex;
pthread_mutex_t loop_capture_mutex;
pthread_mutex_t loop_play_mutex;

unsigned int hw_rate = 96000; /* Sample rate */
snd_pcm_uframes_t hw_period_size = 512; // in frames
uint64_t hw_n_periods = 4; // number of periods

unsigned int loopback_rate = 48000; /* Sample rate */
snd_pcm_uframes_t loopback_period_size = 256; // in frames
uint64_t loopback_n_periods = 4; // number of periods

snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
uint32_t channels = 2;

atomic_bool sound_system_running;
atomic_bool use_loopback;
bool disable_alsa;




extern void sound_process(
    int32_t *input_rx, int32_t *input_mic,
    int32_t *output_speaker, int32_t *output_tx,
	int n_samples);


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

void sound_mixer(char *card_name, char *element, int make_on)
{
    if (disable_alsa)
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
    if(snd_mixer_selem_has_capture_switch(elem)){
	  	snd_mixer_selem_set_capture_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_switch(elem)){
        snd_mixer_selem_set_playback_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_volume(elem)){
        long volume = make_on;
    	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    	snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
    }
    else if (snd_mixer_selem_has_capture_volume(elem)){
        long volume = make_on;
    	snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
    	snd_mixer_selem_set_capture_volume_all(elem, volume * max / 100);
    }
    else if (snd_mixer_selem_is_enumerated(elem)){
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
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&pcm_capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n",
                 device,
                 snd_strerror (e));
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

    while (sound_system_running)
    {
        pthread_mutex_lock(&radio_capture_mutex);

        if ((e = snd_pcm_mmap_readi(pcm_capture_handle, buffer, hw_period_size)) != hw_period_size)
        {
            fprintf (stderr, "read from audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stdout, "overrun\n");
            }
            else if (e < 0) {
                fprintf(stderr,
                        "error from readi: %s\n",
                        snd_strerror(e));
            }  else if (e != hw_period_size) {
                fprintf(stderr,
                        "short read, read %d frames\n", e);

            }
            snd_pcm_prepare (pcm_capture_handle);
            continue;
        }

        for (int j = 0; j < hw_period_size; j++)
        {
            memcpy(&radio[j*sample_size], &buffer[j * sample_size * channels], sample_size);

            // attenuate 25x of the mic
            int32_t *sample = (int32_t *) &buffer[j * sample_size * channels + sample_size];
            *sample /= 25;
            memcpy(&mic[j*sample_size], sample, sample_size);
        }

        write_buffer(radio_to_dsp, radio, buffer_size/2);
        write_buffer(mic_to_dsp, mic, buffer_size/2);

        pthread_mutex_unlock(&radio_capture_mutex);
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
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&pcm_play_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n",
                 device,
                 snd_strerror (e));
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

    while (sound_system_running)
    {

        read_buffer(dsp_to_radio, radio, buffer_size/2);
        read_buffer(dsp_to_speaker, speaker, buffer_size/2);

        pthread_mutex_lock(&radio_play_mutex);
        for (int j = 0; j < hw_period_size; j++)
        {
            memcpy(&buffer[j*sample_size*channels], &speaker[j*sample_size], sample_size);
            memcpy(&buffer[j*sample_size*channels + sample_size], &radio[j*sample_size], sample_size);
        }

    try_again_radio_play:
        if ((e = snd_pcm_mmap_writei(pcm_play_handle, buffer, hw_period_size)) != hw_period_size)
        {
            fprintf (stderr, "write to audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stdout, "overrun\n");

            }
            else if (e < 0) {
                fprintf(stderr,
                        "error from writei: %s\n",
                        snd_strerror(e));
            }  else if (e != hw_period_size) {
                fprintf(stderr,
                        "short write, wrote %d frames\n", e);

            }
            snd_pcm_prepare (pcm_play_handle);
            goto try_again_radio_play;
        }
        pthread_mutex_unlock(&radio_play_mutex);
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
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&loopback_capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n",
                 device,
                 snd_strerror (e));
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

    // apply sw parameters...

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = loopback_period_size * sample_size * channels;

    uint8_t *buffer = malloc(buffer_size);

    snd_pcm_prepare(loopback_capture_handle);
    snd_pcm_drop(loopback_capture_handle);
    snd_pcm_prepare(loopback_capture_handle);

    while (sound_system_running)
    {
        pthread_mutex_lock(&loop_capture_mutex);

        if ((e = snd_pcm_mmap_readi(loopback_capture_handle, buffer, loopback_period_size)) != loopback_period_size)
        {

            fprintf (stderr, "read from audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stdout, "overrun\n");
            }
            else if (e < 0) {
                fprintf(stderr,
                        "error from readi: %s\n",
                        snd_strerror(e));
            }  else if (e != loopback_period_size) {
                fprintf(stderr,
                        "short read, read %d frames\n", e);

            }
            snd_pcm_prepare (loopback_capture_handle);
            continue;
        }

        // attenuate 30db the loopback
        for (int i = 0; i < loopback_period_size; i++)
        {
            int32_t *sample1 = (int32_t *) &buffer[i * sample_size * channels];
            int32_t *sample2 = (int32_t *) &buffer[i * sample_size * channels + sample_size];
            *sample1 /= 90;
            *sample2 /= 90;
        }

        write_buffer(loopback_to_dsp, buffer, buffer_size);

        pthread_mutex_unlock(&loop_capture_mutex);
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
        fprintf (stderr, "Can not allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return NULL;
    }

    if ((e = snd_pcm_open (&loopback_play_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf (stderr, "Can not open audio device %s (%s)\n",
                 device,
                 snd_strerror (e));
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

    while (sound_system_running)
    {
        read_buffer(dsp_to_loopback, buffer, buffer_size);

        pthread_mutex_lock(&loop_play_mutex);
    try_again_loop_play:
        if ((e = snd_pcm_mmap_writei(loopback_play_handle, buffer, loopback_period_size)) != loopback_period_size)
        {
            fprintf (stderr, "write to audio interface %s failed (%s)\n", device, snd_strerror (e));
            if (e == -EPIPE)
            {
                fprintf(stdout, "overrun\n");

            }
            else if (e < 0) {
                fprintf(stderr,
                        "error from readi: %s\n",
                        snd_strerror(e));
            }  else if (e != loopback_period_size) {
                fprintf(stderr,
                        "short read, read %d frames\n", e);

            }
            snd_pcm_prepare (loopback_play_handle);
            goto try_again_loop_play;
        }
        pthread_mutex_unlock(&loop_play_mutex);
    }


    snd_pcm_hw_params_free(hloop_params);
    free(buffer);

    return NULL;
}

void *control_thread_radio(void *device_ptr)
{
    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t hw_buffer_size = hw_period_size * sample_size; // single channel
    uint8_t *buffer_radio_to_dsp = malloc(hw_buffer_size);
    uint8_t *buffer_mic_to_dsp = malloc(hw_buffer_size);


    while (1)
    {
        read_buffer(radio_to_dsp, buffer_radio_to_dsp, hw_buffer_size);
        read_buffer(mic_to_dsp, buffer_mic_to_dsp, hw_buffer_size); // the samplerate is half


        write_buffer(dsp_to_radio, buffer_radio_to_dsp, hw_buffer_size);
        write_buffer(dsp_to_speaker, buffer_mic_to_dsp, hw_buffer_size);
    }

}

void *control_thread_loopback(void *device_ptr)
{
    int sample_size = snd_pcm_format_width(format) / 8;

    uint32_t loop_buffer_size = loopback_period_size * sample_size * channels;
    uint8_t *buffer_loop_to_dsp = malloc(loop_buffer_size);

    while (1)
    {
        read_buffer(loopback_to_dsp, buffer_loop_to_dsp, loop_buffer_size);

        write_buffer(dsp_to_loopback, buffer_loop_to_dsp, loop_buffer_size);
    }
}

void *control_thread(void *device_ptr)
{
    int i_need_1024_frames = 1024;

    int sample_size = snd_pcm_format_width(format) / 8;
    //uint32_t hw_buffer_size = hw_period_size * sample_size;
    uint32_t hw_buffer_size = i_need_1024_frames * sample_size;
    uint8_t *buffer_radio_to_dsp = malloc(hw_buffer_size);
    uint8_t *buffer_mic_to_dsp = malloc(hw_buffer_size);

    //uint32_t loop_buffer_size = loopback_period_size * sample_size;
    uint32_t loop_buffer_size = (i_need_1024_frames / 2) * sample_size * channels; // the samplerate is half, 2 ch
    uint8_t *buffer_loop_to_dsp = malloc(loop_buffer_size);
    // uint8_t *buffer_loop_to_dsp_upsampled = malloc(hw_buffer_size);


    int32_t *input_rx; int32_t *input_mic; int32_t *output_speaker; int32_t *output_tx;

    output_tx = (int32_t *)malloc(hw_buffer_size);
    output_speaker = (int32_t *)malloc(hw_buffer_size);

    while (1)
    {
        read_buffer(radio_to_dsp, buffer_radio_to_dsp, hw_buffer_size);
        read_buffer(mic_to_dsp, buffer_mic_to_dsp, hw_buffer_size); // the samplerate is half
        if (use_loopback)
            read_buffer(loopback_to_dsp, buffer_loop_to_dsp, loop_buffer_size);
        else
            clear_buffer(loopback_to_dsp);

        if (use_loopback)
        {
#if 0 // aaaaah, we need to upsample this...
            j = 0;
            for (int i = 0; i < i_need_1024_frames; i = i + 2){
                memcpy(buffer_loop_to_dsp_upsampled + i * sample_size, buffer_loop_to_dsp + j * sample_size, sample_size);
                memcpy(buffer_loop_to_dsp_upsampled + i * sample_size + sample_size, buffer_loop_to_dsp + j * sample_size, sample_size);
                j++;
            }
#endif
            input_rx = (int32_t *)buffer_radio_to_dsp;
            input_mic = (int32_t *)buffer_loop_to_dsp;
        }
        else
        {
            input_rx = (int32_t *)buffer_radio_to_dsp;
            input_mic = (int32_t *)buffer_mic_to_dsp;
        }

        sound_process(input_rx, input_mic, output_speaker, output_tx, i_need_1024_frames);

        write_buffer(dsp_to_loopback, (uint8_t *)output_speaker, hw_buffer_size);             // good ol' mono->stereo rate halving
        write_buffer(dsp_to_radio, (uint8_t *)output_tx, hw_buffer_size);
        write_buffer(dsp_to_speaker, (uint8_t *)output_speaker, hw_buffer_size);
    }
}

void sound_input(int loop){
    if (loop)
        use_loopback = true;
    else
        use_loopback = false;
}


void clear_buffers(){

    pthread_mutex_lock(&radio_capture_mutex);
    pthread_mutex_lock(&radio_play_mutex);
    pthread_mutex_lock(&loop_capture_mutex);
    pthread_mutex_lock(&loop_play_mutex);

    snd_pcm_prepare(pcm_capture_handle);
    snd_pcm_drop(pcm_capture_handle);
    snd_pcm_prepare(pcm_capture_handle);

    snd_pcm_prepare(pcm_play_handle);
    snd_pcm_drop(pcm_play_handle);
    snd_pcm_prepare(pcm_play_handle);

    snd_pcm_prepare(loopback_capture_handle);
    snd_pcm_drop(loopback_capture_handle);
    snd_pcm_prepare(loopback_capture_handle);

    snd_pcm_prepare(loopback_play_handle);
    snd_pcm_drop(loopback_play_handle);
    snd_pcm_prepare(loopback_play_handle);


    clear_buffer(radio_to_dsp);
    clear_buffer(dsp_to_radio);
    clear_buffer(mic_to_dsp);
    clear_buffer(dsp_to_speaker);
    clear_buffer(dsp_to_loopback);
    clear_buffer(loopback_to_dsp);

    pthread_mutex_unlock(&radio_capture_mutex);
    pthread_mutex_unlock(&radio_play_mutex);
    pthread_mutex_unlock(&loop_capture_mutex);
    pthread_mutex_unlock(&loop_play_mutex);


}

// here we start the sound system and create all the threads
void sound_system_start()
{
    pthread_t radio_capture, radio_playback;
    pthread_t loop_capture, loop_playback;

    int rc = 0;
    rc |= pthread_mutex_init(&radio_capture_mutex, NULL);
    rc |= pthread_mutex_init(&radio_play_mutex, NULL);
    rc |= pthread_mutex_init(&loop_capture_mutex, NULL);
    rc |= pthread_mutex_init(&loop_play_mutex, NULL);

    if (rc != 0)
    {
        fprintf(stderr, "Error in mutex init\n");
        return;
    }

    sound_system_running = true;

#if 1
    pthread_t control_tid;
    pthread_create( &control_tid, NULL, control_thread, NULL);
#else
    pthread_t control_radio_tid;
    pthread_create( &control_radio_tid, NULL, control_thread_radio, NULL);
    pthread_t control_loop_tid;
    pthread_create( &control_loop_tid, NULL, control_thread_loopback, NULL);
#endif

    pthread_create( &radio_capture, NULL, radio_capture_thread, (void*)radio_capture_dev);
	pthread_create( &radio_playback, NULL, radio_playback_thread, (void*)radio_playback_dev);
    // wait here for sound bringup
    pthread_create( &loop_capture, NULL, loop_capture_thread, (void*)loop_capture_dev);
	pthread_create( &loop_playback, NULL, loop_playback_thread, (void*)loop_playback_dev);

	struct sched_param sch;
	sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(radio_capture, SCHED_FIFO, &sch);
	pthread_setschedparam(radio_playback, SCHED_FIFO, &sch);
	pthread_setschedparam(loop_capture, SCHED_FIFO, &sch);
	pthread_setschedparam(loop_playback, SCHED_FIFO, &sch);


}
