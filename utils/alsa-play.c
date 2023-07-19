/*

This example reads standard from input and writes
to the default PCM device for 5 seconds of data.

*/

/* Use the newer ALSA API */
// #define ALSA_PCM_NEW_HW_PARAMS_API

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

int main(int argc, char *argv[]) {
  long loops;
  int rc;
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *params;
  unsigned int val, val2;
  int dir;
  char *buffer;


  snd_pcm_uframes_t period_size = 256; // in frames
  uint64_t n_periods = 4; // number of periods
  unsigned int rate = 96000;
  uint32_t channels = 2;
  snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;


  /* Open PCM device for playback. */
  rc = snd_pcm_open(&handle, argv[1],
                    SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    fprintf(stderr,
            "unable to open pcm device: %s\n",
            snd_strerror(rc));
    exit(1);
  }

  fprintf(stdout, "audio interface opened\n");

  /* Allocate a hardware parameters object. */
  snd_pcm_hw_params_alloca(&params);

  fprintf(stdout, "hw_params allocated\n");

  /* Fill it in with default values. */
  snd_pcm_hw_params_any(handle, params);

  fprintf(stdout, "hw_params initialized\n");


  /* Set the desired hardware parameters. */

  /* Interleaved mode */
  snd_pcm_hw_params_set_access(handle, params,
                      SND_PCM_ACCESS_MMAP_INTERLEAVED);

  fprintf(stdout, "hw_params access setted\n");

  /* Signed 16-bit little-endian format */
  snd_pcm_hw_params_set_format(handle, params,
                              format);

  fprintf(stdout, "hw_params format setted\n");

  /* channels */
  snd_pcm_hw_params_set_channels(handle, params, channels);

  /* sampling rate */
  snd_pcm_hw_params_set_rate_near(handle, params,
                                  &rate, &dir);

  /* Set period size. */
  snd_pcm_hw_params_set_period_size_near(handle,
                              params, &period_size, &dir);

/* Set number of periods. Periods used to be called fragments. */
  snd_pcm_hw_params_set_periods(handle, params, n_periods, 0);


  /* Write the parameters to the driver */
  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0) {
    fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(rc));
    exit(1);
  }

  fprintf(stdout, "hw_params setted\n");


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

  snd_pcm_hw_params_get_period_time(params,
                                    &val, &dir);
  printf("period time = %d us\n", val);

  snd_pcm_hw_params_get_period_size(params,
                                    &period_size, &dir);
  printf("period size = %d frames\n", (int)period_size);

  snd_pcm_hw_params_get_buffer_time(params,
                                    &val, &dir);
  printf("buffer time = %d us\n", val);

  snd_pcm_hw_params_get_buffer_size(params,
                         (snd_pcm_uframes_t *) &val);
  printf("buffer size = %d frames\n", val);

  snd_pcm_hw_params_get_periods(params, &val, &dir);
  printf("periods per buffer = %d frames\n", val);

  snd_pcm_hw_params_get_rate_numden(params,
                                    &val, &val2);
  printf("exact rate = %d/%d bps\n", val, val2);

  val = snd_pcm_hw_params_get_sbits(params);
  printf("significant bits = %d\n", val);

  snd_pcm_hw_params_get_tick_time(params,
                                  &val, &dir);
  printf("tick time = %d us\n", val);

  val = snd_pcm_hw_params_is_batch(params);
  printf("is batch = %d\n", val);

  val = snd_pcm_hw_params_is_block_transfer(params);
  printf("is block transfer = %d\n", val);

  val = snd_pcm_hw_params_is_double(params);
  printf("is double = %d\n", val);

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


  /* Use a buffer large enough to hold one period */
  snd_pcm_hw_params_get_period_size(params, &period_size,
                                    &dir);


  rc = snd_pcm_prepare (handle);

  if (rc < 0){
      fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
               snd_strerror (rc));
      exit (1);
  }

  fprintf(stdout, "audio interface prepared\n");

  int sample_size = snd_pcm_format_width(format) / 8;
  uint32_t buffer_size = period_size * sample_size * channels;


  fprintf(stdout, "buffer size %u\n", buffer_size);

  buffer = malloc(buffer_size);

  fprintf(stdout, "buffer allocated\n");

  /* We want to loop for 5 seconds */
  snd_pcm_hw_params_get_period_time(params,
                                    &val, &dir);
  /* 5 seconds in microseconds divided by
   * period time */
  loops = 5000000 / val;

  while (loops > 0) {
    loops--;
    rc = read(0, buffer, buffer_size);
    if (rc == 0) {
      fprintf(stderr, "end of file on input\n");
      break;
    } else if (rc != buffer_size) {
      fprintf(stderr,
              "short read: read %d bytes\n", rc);
    }
    rc = snd_pcm_mmap_writei(handle, buffer, period_size);
    if (rc == -EPIPE) {
      /* EPIPE means underrun */
      fprintf(stderr, "underrun occurred\n");
      snd_pcm_prepare(handle);
    } else if (rc < 0) {
      fprintf(stderr,
              "error from writei: %s\n",
              snd_strerror(rc));
    }  else if (rc != (int)period_size) {
      fprintf(stderr,
              "short write, write %d frames\n", rc);
    }
  }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);

  return 0;
}
