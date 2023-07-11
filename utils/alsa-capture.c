/*
  A Minimal Capture Program
  This program opens an audio interface for capture, configures it for
  stereo, 16 bit, 44.1kHz, interleaved conventional read/write
  access. Then its reads a chunk of random data from it, and exits. It
  isn't meant to be a real program.
  From on Paul David's tutorial : http://equalarea.com/paul/alsa-audio.html
  Fixes rate and buffer problems
  sudo apt-get install libasound2-dev
  gcc -o alsa-record-example -lasound alsa-record-example.c && ./alsa-record-example hw:0
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

// latency = periodsize * periods / (rate * bytes_per_frame)
int main (int argc, char *argv[])
{
    int i;
    int err;
    char *buffer;
    uint64_t period_size = 256; // in frames
    uint64_t n_periods = 4; // number of periods

    unsigned int rate = 96000;
    uint32_t channels = 2;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
	snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;


    if ((err = snd_pcm_open (&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n",
                 argv[1],
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "audio interface opened\n");

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params allocated\n");

    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params initialized\n");

    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf (stderr, "cannot set access type (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params access setted\n");

    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
        fprintf (stderr, "cannot set sample format (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params format setted\n");



    if ((err = snd_pcm_hw_params_test_period_size (capture_handle, hw_params, period_size, 0)) < 0) {
        fprintf (stderr, "period size of %lu not good (%s)\n", period_size,
                 snd_strerror (err));
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params, &period_size, 0)) < 0) {
        fprintf (stderr, "cannot set period size (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

/* Set number of periods. Periods used to be called fragments. */
    if ((err = snd_pcm_hw_params_set_periods(capture_handle, hw_params, n_periods, 0)) < 0){
        fprintf(stderr, "Error setting periods.\n");
        return(-1);
    }


    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
        fprintf (stderr, "cannot set sample rate (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params rate setted\n");

    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) {
        fprintf (stderr, "cannot set channel count (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params channels setted\n");

    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot set parameters (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "hw_params setted\n");

    snd_pcm_hw_params_free (hw_params);

    fprintf(stdout, "hw_params freed\n");

    if ((err = snd_pcm_prepare (capture_handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                 snd_strerror (err));
        exit (1);
    }

    fprintf(stdout, "audio interface prepared\n");

    /* Display information about the PCM interface */
    unsigned int val, val2;
    printf("PCM capture_handle name = '%s'\n",
           snd_pcm_name(capture_handle));

    printf("PCM state = %s\n",
           snd_pcm_state_name(snd_pcm_state(capture_handle)));

    snd_pcm_hw_params_get_access(hw_params,
                                 (snd_pcm_access_t *) &val);
    printf("access type = %s\n",
           snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(hw_params, &val);
    printf("format = '%s' (%s)\n",
           snd_pcm_format_name((snd_pcm_format_t)val),
           snd_pcm_format_description(
               (snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(hw_params,
                                    (snd_pcm_subformat_t *)&val);
    printf("subformat = '%s' (%s)\n",
           snd_pcm_subformat_name((snd_pcm_subformat_t)val),
           snd_pcm_subformat_description(
               (snd_pcm_subformat_t)val));

    snd_pcm_hw_params_get_channels(hw_params, &val);
    printf("channels = %d\n", val);

    snd_pcm_hw_params_get_rate(hw_params, &val, 0);
    printf("rate = %d bps\n", val);

    snd_pcm_hw_params_get_period_time(hw_params,
                                      &val, 0);
    printf("period time = %d us\n", val);

    snd_pcm_uframes_t frames;
    snd_pcm_hw_params_get_period_size(hw_params,
                                      &frames, 0);
    printf("period size = %d frames\n", (int)frames);

    snd_pcm_hw_params_get_buffer_time(hw_params,
                                      &val, 0);
    printf("buffer time = %d us\n", val);

    snd_pcm_hw_params_get_buffer_size(hw_params,
                                      (snd_pcm_uframes_t *) &val);
    printf("buffer size = %d frames\n", val);

    snd_pcm_hw_params_get_periods(hw_params, &val, 0);
    printf("periods per buffer = %d frames\n", val);


    snd_pcm_hw_params_get_rate_numden(hw_params,
                                      &val, &val2);
    printf("exact rate = %d/%d bps\n", val, val2);

    val = snd_pcm_hw_params_get_sbits(hw_params);
    printf("significant bits = %d\n", val);

    snd_pcm_hw_params_get_tick_time(hw_params,
                                    &val, 0);
    printf("tick time = %d us\n", val);

    val = snd_pcm_hw_params_is_batch(hw_params);
    printf("is batch = %d\n", val);

    val = snd_pcm_hw_params_is_block_transfer(hw_params);
    printf("is block transfer = %d\n", val);

    val = snd_pcm_hw_params_is_double(hw_params);
    printf("is double = %d\n", val);

    val = snd_pcm_hw_params_is_half_duplex(hw_params);
    printf("is half duplex = %d\n", val);

    val = snd_pcm_hw_params_is_joint_duplex(hw_params);
    printf("is joint duplex = %d\n", val);

    val = snd_pcm_hw_params_can_overrange(hw_params);
    printf("can overrange = %d\n", val);

    val = snd_pcm_hw_params_can_mmap_sample_resolution(hw_params);
    printf("can mmap = %d\n", val);

    val = snd_pcm_hw_params_can_pause(hw_params);
    printf("can pause = %d\n", val);

    val = snd_pcm_hw_params_can_resume(hw_params);
    printf("can resume = %d\n", val);

    val = snd_pcm_hw_params_can_sync_start(hw_params);
    printf("can sync start = %d\n", val);


    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = period_size * sample_size * channels;
    fprintf(stdout, "buffer size %u\n", buffer_size);

    buffer = malloc(buffer_size);

    fprintf(stdout, "buffer allocated\n");

    uint8_t *left = (uint8_t *) malloc(buffer_size/2);
    uint8_t *right = (uint8_t *) malloc(buffer_size/2);

    FILE *left_fp = fopen("left.raw", "w");
    FILE *right_fp = fopen("right.raw", "w");

    for (i = 0; i < 1000000; ++i) {
        if ((err = snd_pcm_readi (capture_handle, buffer, period_size)) != period_size) {
            fprintf (stderr, "read from audio interface failed (%s)\n",
                     snd_strerror (err));
            if (err == -EPIPE)
            {
                fprintf(stdout, "overrun\n");
                snd_pcm_prepare (capture_handle);
            }
            else if (err < 0) {
                fprintf(stderr,
                        "error from readi: %s\n",
                        snd_strerror(err));
                exit (1);
            }  else if (err != period_size) {
                fprintf(stderr,
                        "short read, read %d frames\n", err);
            }

        }

        for (int j = 0; j < period_size; j++)
        {
            memcpy(&left[j*sample_size], &buffer[j * sample_size * channels], sample_size);
            memcpy(&right[j*sample_size], &buffer[j * sample_size * channels + sample_size], sample_size);
        }

        // fprintf(stdout, "read %d done\n", i);
        fwrite(left, buffer_size/2, 1, left_fp);
        fwrite(right, buffer_size/2, 1, right_fp);

    }

    free(buffer);
    free(left);
    free(right);

    fclose(left_fp);
    fclose(right_fp);

    fprintf(stdout, "buffer freed\n");

    snd_pcm_close (capture_handle);
    fprintf(stdout, "audio interface closed\n");

    exit (0);
}
