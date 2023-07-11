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

    uint32_t buffer_size = period_size * (snd_pcm_format_width(format) / 8) * channels;
    fprintf(stdout, "buffer size %u\n", buffer_size);

    buffer = malloc(buffer_size);

    fprintf(stdout, "buffer allocated\n");


    for (i = 0; i < 1000000; ++i) {
        if ((err = snd_pcm_readi (capture_handle, buffer, period_size)) < 0) {
            fprintf (stderr, "read from audio interface failed (%s)\n",
                     snd_strerror (err));
            if (err == -EPIPE)
            {
                fprintf(stdout, "overrun\n");
                snd_pcm_prepare (capture_handle);
            }
            else
                exit (1);
        }
        fprintf(stdout, "read %d done\n", i);
    }

    free(buffer);

    fprintf(stdout, "buffer freed\n");

    snd_pcm_close (capture_handle);
    fprintf(stdout, "audio interface closed\n");

    exit (0);
}
