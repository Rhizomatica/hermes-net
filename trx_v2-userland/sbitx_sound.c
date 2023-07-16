// Code based on original Ashhar sbitx code, with many changes

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <complex.h>
#include <fftw3.h>
#include <time.h>
#include <stdbool.h>

#include "sbitx_alsa.h"
#include "sound.h"
#include "wiringPi.h"
#include "sdr.h"



extern bool disable_alsa;

unsigned int hw_rate = 96000; /* Sample rate */
snd_pcm_uframes_t hw_period_size = 1024; // in frames
uint64_t hw_n_periods = 2; // number of periods

unsigned int loopback_rate = 48000; /* Sample rate */
snd_pcm_uframes_t loopback_period_size = 512; // in frames
uint64_t loopback_n_periods = 2; // number of periods

snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
uint32_t channels = 2;

static snd_pcm_t *pcm_play_handle=0;   	//handle for the pcm device
static snd_pcm_t *pcm_capture_handle=0;   	//handle for the pcm device
static snd_pcm_t *loopback_play_handle=0;   	//handle for the pcm device
static snd_pcm_t *loopback_capture_handle=0;   	//handle for the pcm device

static snd_pcm_stream_t play_stream = SND_PCM_STREAM_PLAYBACK;	//playback stream
static snd_pcm_stream_t capture_stream = SND_PCM_STREAM_CAPTURE;	//playback stream

static uint32_t exact_rate;   /* Sample rate returned by */
static int	sound_thread_continue = 0;
pthread_t sound_thread, loopback_thread;

#define LOOPBACK_LEVEL_DIVISOR 8				// Constant used to reduce audio level to the loopback channel (FLDIGI)


static pthread_barrier_t barrier;

int use_virtual_cable = 0;

struct Queue qloop;



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

/*		if (elem)
			puts("Element found.");	
	*/
    //find out if the his element is capture side or plaback
    if(snd_mixer_selem_has_capture_switch(elem)){
        //puts("this is a capture switch.");
	  	snd_mixer_selem_set_capture_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_switch(elem)){
		//	puts("this is a playback switch.");
        snd_mixer_selem_set_playback_switch_all(elem, make_on);
    }
    else if (snd_mixer_selem_has_playback_volume(elem)){
        //puts("this is  playback volume");
        long volume = make_on;
    	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    	snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
    }	
    else if (snd_mixer_selem_has_capture_volume(elem)){
		//	puts("this is a capture volume");
        long volume = make_on;
    	snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
    	snd_mixer_selem_set_capture_volume_all(elem, volume * max / 100);
    }
    else if (snd_mixer_selem_is_enumerated(elem)){
//			puts("TBD: this is an enumerated capture element");
        snd_mixer_selem_set_enum_item(elem, 0, make_on);
    }
    snd_mixer_close(handle);
}

/* this function should be called just once in the application process.
Calling it frequently will result in more allocation of hw_params memory blocks
without releasing them.
The list of PCM devices available on any platform can be found by running
	aplay -L 
We have to pass the id of one of those devices to this function.
The sequence of the alsa functions must be maintained for this to work consistently

It returns a -1 if the device didn't open. The error message is on stderr.

IMPORTANT:
The sound is playback is carried on in a non-blocking way  

*/
int sound_start_play(char *device){
    int e;

    fprintf(stderr, "ALSA Playback device is: %s\n", device);

    snd_pcm_hw_params_t *hwparams;

    if ((e = snd_pcm_hw_params_malloc (&hwparams)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return -1;
    }

    e = snd_pcm_open(&pcm_play_handle, device, play_stream, SND_PCM_NONBLOCK);
	
	if (e < 0) {
		fprintf(stderr, "Error opening PCM playback device %s: %s\n", device, snd_strerror(e));
		return -1;
	}

	e = snd_pcm_hw_params_any(pcm_play_handle, hwparams);

	if (e < 0) {
		fprintf(stderr, "*Error getting hw playback params (%d)\n", e);
		return(-1);
	}

	e = snd_pcm_hw_params_set_access(pcm_play_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		fprintf(stderr, "*Error setting playback access.\n");
		return(-1);
	}

	e = snd_pcm_hw_params_set_format(pcm_play_handle, hwparams, format);
	if (e < 0) {
		fprintf(stderr, "*Error setting plyaback format.\n");
		return(-1);
	}

	exact_rate = hw_rate;
	e = snd_pcm_hw_params_set_rate_near(pcm_play_handle, hwparams, &exact_rate, 0);
	if ( e< 0) {
		fprintf(stderr, "Error setting playback rate.\n");
		return(-1);
	}
	if (hw_rate != exact_rate)
		fprintf(stderr, "*The playback rate %d changed to %d Hz\n", hw_rate, exact_rate);

	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(pcm_play_handle, hwparams, channels)) < 0) {
		fprintf(stderr, "*Error setting playback channels.\n");
		return(-1);
	}

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(pcm_play_handle,hwparams, &hw_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return (-1);
    }

    /* nr. of periods */
	if ((e = snd_pcm_hw_params_set_periods(pcm_play_handle, hwparams, hw_n_periods, 0)) < 0) {
		fprintf(stderr, "*Error setting playback periods.\n");
		return(-1);
	}

	if (snd_pcm_hw_params(pcm_play_handle, hwparams) < 0) {
		fprintf(stderr, "*Error setting playback HW params.\n");
		return(-1);
	}

    printf("============= REPORT RADIO PLAYBACK DEVICE %s =============\n", device);

    show_alsa(pcm_play_handle, hwparams);

    //snd_pcm_hw_params_free(hwparams);

	return 0;
}


int sound_start_loopback_capture(char *device){
    int e;

    fprintf(stderr, "ALSA Loopback Capture device at: %s\n", device);

    snd_pcm_hw_params_t *hloop_params;

    if ((e = snd_pcm_hw_params_malloc (&hloop_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return -1;
    }

	snd_pcm_open(&loopback_capture_handle, device, capture_stream, 0);
	
	if (e < 0) {
		fprintf(stderr, "Err: Opening loop capture  %s: %s\n", device, snd_strerror(e));
		return -1;
	}

	e = snd_pcm_hw_params_any(loopback_capture_handle, hloop_params);

	if (e < 0) {
		fprintf(stderr, "*Error setting capture access (%d)\n", e);
		return(-1);
	}

	e = snd_pcm_hw_params_set_access(loopback_capture_handle, hloop_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		fprintf(stderr, "*Error setting capture access.\n");
		return(-1);
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(loopback_capture_handle, hloop_params, format);
	if (e < 0) {
		fprintf(stderr, "*Error setting loopback capture format.\n");
		return(-1);
	}

	exact_rate = loopback_rate;
	//printf("Setting loopback capture rate to %d\n", exact_rate);
	e = snd_pcm_hw_params_set_rate_near(loopback_capture_handle, hloop_params, &exact_rate, 0);
	if ( e< 0) {
		fprintf(stderr, "*Error setting loopback capture rate.\n");
		return(-1);
	}

	if (loopback_rate != exact_rate)
		fprintf(stderr, "#The loopback capture rate set to %d Hz\n", exact_rate);

	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(loopback_capture_handle, hloop_params, channels)) < 0) {
		fprintf(stderr, "*Error setting loopback capture channels.\n");
		return(-1);
	}

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(loopback_capture_handle, hloop_params, &loopback_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return (-1);
    }

	if ((e = snd_pcm_hw_params_set_periods(loopback_capture_handle, hloop_params, loopback_n_periods, 0)) < 0) {
		fprintf(stderr, "*Error setting loopback capture periods.\n");
		return(-1);
	}


	if (snd_pcm_hw_params(loopback_capture_handle, hloop_params) < 0) {
		fprintf(stderr, "*Error setting capture HW params.\n");
		return(-1);
	}

    printf("============= REPORT LOOPBACK CAPTURE DEVICE %s =============\n", device);
#if 0
    snd_pcm_sw_params_t *sloop_params;

	snd_pcm_sw_params_malloc(&sloop_params);
	if((e = snd_pcm_sw_params_current(loopback_capture_handle, sloop_params)) < 0){
		fprintf(stderr, "Error getting current loopback capture sw params : %s\n", snd_strerror(e));
		return (-1);
	}
	
	if ((e = snd_pcm_sw_params_set_start_threshold(loopback_capture_handle, sloop_params, 15)) < 0){
		fprintf(stderr, "Unable to set threshold mode for loopback capture\n");
	} 
	
	if ((e = snd_pcm_sw_params_set_stop_threshold(loopback_capture_handle, sloop_params, 1)) < 0){

		fprintf(stderr, "Unable to set stop threshold for loopback  capture\n");
	}

    unsigned int val;
    e = snd_pcm_sw_params_get_silence_size(sloop_params, (snd_pcm_uframes_t *) &val);
    if(!e)
        printf("sw silence size (frames): %u\n", val);
    //snd_pcm_sw_params_set_silence_size

    e = snd_pcm_sw_params_get_silence_threshold(sloop_params, (snd_pcm_uframes_t *) &val);
    if(!e)
        printf("silence threshold (frames): %u\n", val);
    //   snd_pcm_sw_params_set_silence_threshold
//#if 0

	if ((e = snd_pcm_sw_params(loopback_capture_handle,sloop_params)) < 0){
		fprintf(stderr, "Unable to set alsa sw parameters\n");
	}
#endif

    show_alsa(loopback_capture_handle, hloop_params);

    //snd_pcm_sw_params_free(sloop_params);

    //snd_pcm_hw_params_free(hloop_params);

    return 0;
}

/*
The capture is opened in a blocking mode, the read function will block until 
there are enough samples to return a block.
This ensures that the blocks are returned in perfect timing with the codec's clock
Once you process these captured samples and send them to the playback device, you
just wait for the next block to arrive 
*/

int sound_start_capture(char *device){
    int e;
    snd_pcm_hw_params_t *hwparams;

    if ((e = snd_pcm_hw_params_malloc (&hwparams)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return -1;
    }

	e = snd_pcm_open(&pcm_capture_handle, device, capture_stream, 0);

    fprintf(stderr, "ALSA Capture device is: %s\n", device);

	if (e < 0) {
		fprintf(stderr, "Error opening PCM capture device %s: %s\n", device, snd_strerror(e));
		return -1;
	}

	e = snd_pcm_hw_params_any(pcm_capture_handle, hwparams);

	if (e < 0) {
		fprintf(stderr, "*Error setting capture access (%d)\n", e);
		return(-1);
	}

	e = snd_pcm_hw_params_set_access(pcm_capture_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		fprintf(stderr, "*Error setting capture access.\n");
		return(-1);
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(pcm_capture_handle, hwparams, format);
	if (e < 0) {
		fprintf(stderr, "*Error setting capture format.\n");
		return(-1);
	}


	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = hw_rate;
	e = snd_pcm_hw_params_set_rate_near(pcm_capture_handle, hwparams, &exact_rate, 0);
	if ( e< 0) {
		fprintf(stderr, "*Error setting capture rate.\n");
		return(-1);
	}

	if (hw_rate != exact_rate)
		fprintf(stderr, "#The capture rate %d changed to %d Hz\n", hw_rate, exact_rate);


	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(pcm_capture_handle, hwparams, channels)) < 0) {
		fprintf(stderr, "*Error setting capture channels.\n");
		return(-1);
	}

    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(pcm_capture_handle,hwparams, &hw_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return (-1);
    }

	/* Set number of periods. Periods used to be called fragments. */ 
	if ((e = snd_pcm_hw_params_set_periods(pcm_capture_handle, hwparams, hw_n_periods, 0)) < 0) {
		fprintf(stderr, "*Error setting capture periods.\n");
		return(-1);
	}


	if (snd_pcm_hw_params(pcm_capture_handle, hwparams) < 0) {
		fprintf(stderr, "*Error setting capture HW params.\n");
		return(-1);
	}

    printf("============= REPORT RADIO CAPTURE DEVICE %s =============\n", device);

    show_alsa(pcm_play_handle, hwparams);

    //snd_pcm_hw_params_free(hwparams);

	return 0;
}

int sound_start_loopback_play(char *device){
    int e;
    fprintf(stderr, "ALSA Loopback Playback device at: %s\n", device);


    snd_pcm_hw_params_t *hloop_params;

    if ((e = snd_pcm_hw_params_malloc (&hloop_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (e));
        return -1;
    }

	//printf ("opening audio rx stream to %s\n", device); 
	snd_pcm_open(&loopback_play_handle, device, play_stream, SND_PCM_NONBLOCK);
	
	if (e < 0) {
		fprintf(stderr, "Error opening loopback playback device %s: %s\n", device, snd_strerror(e));
		return -1;
	}

	e = snd_pcm_hw_params_any(loopback_play_handle, hloop_params);

	if (e < 0) {
		fprintf(stderr, "*Error getting loopback playback params (%d)\n", e);
		return(-1);
	}

	e = snd_pcm_hw_params_set_access(loopback_play_handle, hloop_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		fprintf(stderr, "*Error setting loopback access.\n");
		return(-1);
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(loopback_play_handle, hloop_params, format);
	if (e < 0) {
		fprintf(stderr, "*Error setting loopback format.\n");
		return(-1);
	}

	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = loopback_rate;
	e = snd_pcm_hw_params_set_rate_near(loopback_play_handle, hloop_params, &exact_rate, 0);
	if ( e< 0) {
		fprintf(stderr, "Error setting playback rate.\n");
		return(-1);
	}
	if (loopback_rate != exact_rate)
		fprintf(stderr, "*The loopback playback rate %d changed to %d Hz\n", loopback_rate, exact_rate);


	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(loopback_play_handle, hloop_params, channels)) < 0) {
		fprintf(stderr, "*Error setting playback channels.\n");
		return(-1);
	}


    /* Set period size. */
    if ((e = snd_pcm_hw_params_set_period_size_near(loopback_play_handle, hloop_params, &loopback_period_size, 0)) < 0)
    {
        fprintf (stderr, "cannot set period size (%s)\n",snd_strerror (e));
        return (-1);
    }


	if ((e = snd_pcm_hw_params_set_periods(loopback_play_handle, hloop_params, loopback_n_periods, 0)) < 0) {
		fprintf(stderr, "*Error setting playback periods.\n");
		return(-1);
	}

	if (snd_pcm_hw_params(loopback_play_handle, hloop_params) < 0) {
		fprintf(stderr, "*Error setting loopback playback HW params.\n");
		return(-1);
	}

    printf("============= REPORT LOOPBACK PLAYBACK DEVICE %s =============\n", device);

    show_alsa(loopback_play_handle, hloop_params);

    //snd_pcm_hw_params_free(hloop_params);

	return 0;
}

// this is only a test process to be substituted to try loopback 
// it was used to debug timing errors
void sound_process2(int32_t *input_i, int32_t *input_q, int32_t *output_i, int32_t *output_q, int n_samples){
 
	for (int i= 0; i < n_samples; i++){
		output_i[i] = input_q[i];
		output_q[i] = 0;
	}	
}

void sound_stop(){
	snd_pcm_drop(pcm_play_handle);
	snd_pcm_drain(pcm_play_handle);

	snd_pcm_drop(pcm_capture_handle);
	snd_pcm_drain(pcm_capture_handle);
}

static struct timespec gettime_now;
static long int last_time = 0;
int32_t	resample_in[10000];
int32_t	resample_out[10000];

int last_second = 0;
int nsamples = 0;
int	played_samples = 0;

void sound_loop(){
	int32_t	 *line_out, *data_in, *data_out,
        *input_i, *output_i, *input_q, *output_q;
    int pcmreturn, i, j;
    int frames;

	//we allocate enough for two channels of int32_t sized samples
    int sample_size = snd_pcm_format_width(format) / 8;
    // uint32_t buffer_size = hw_period_size * sample_size * channels;
    // frames = hw_period_size;
    frames = 1024;
    uint32_t buffer_size = 1024 * sample_size * channels;

	//we allocate enough for two channels of int32_t sized samples	
    data_in = (int32_t *)malloc(buffer_size);
    line_out = (int32_t *)malloc(buffer_size);
    data_out = (int32_t *)malloc(buffer_size);
    input_i = (int32_t *)malloc(buffer_size);
    output_i = (int32_t *)malloc(buffer_size);
    input_q = (int32_t *)malloc(buffer_size);
    output_q = (int32_t *)malloc(buffer_size);


	
    snd_pcm_prepare(pcm_play_handle);
    snd_pcm_prepare(loopback_play_handle);
    snd_pcm_writei(pcm_play_handle, data_out, frames);
    snd_pcm_writei(pcm_play_handle, data_out, frames);

    pthread_barrier_wait(&barrier);

    //Note: the virtual cable samples queue should be flushed at the start of tx
    qloop.stall = 1;

    while(sound_thread_continue) {

		//restart the pcm capture if there is an error reading the samples
		//this is opened as a blocking device, hence we derive accurate timing 

		last_time = gettime_now.tv_nsec/1000;


		while ((pcmreturn = snd_pcm_readi(pcm_capture_handle, data_in, frames)) < 0){
			snd_pcm_prepare(pcm_capture_handle);
			//putchar('=');
		}
		i = 0; 
		j = 0;
		int ret_card = pcmreturn;
        if (use_virtual_cable){

			//printf(" we have %d in qloop, writing now\n", q_length(&qloop));
			// if don't we have enough to last two iterations loop back...
			if (q_length(&qloop) < pcmreturn){
//				puts(" skipping");
				continue;
			}
	
			//copy 1024 samples from the queue.
			i = 0;
			j = 0;

			
			for (int samples  = 0; samples < 1024; samples++){
				int32_t s = q_read(&qloop);
				input_i[j] = input_q[j] = s;
				j++; 
			}
			played_samples += 1024;
		}
		else {
			while (i < ret_card){
				input_i[i] = data_in[j++]/2;
				input_q[i] = data_in[j++]/2;
				i++;
			}
		}

		//printf("%d %ld %d\n", count++, nsamples, pcmreturn);
		sound_process(input_i, input_q, output_i, output_q, ret_card);

		i = 0; 
		j = 0;	
		while (i < ret_card){
			data_out[j++] = output_i[i];
			data_out[j++] = output_q[i++];
		}

/*
// This is the original pcm play write routine, now commented out.
while ((pcmreturn = snd_pcm_writei(pcm_play_handle,
data_out, frames)) < 0) {
snd_pcm_prepare(pcm_play_handle);
}
*/

        // This is the new pcm play write routine

        int framesize = ret_card;
        int offset = 0;
		
        while(framesize > 0)
        {
            pcmreturn = snd_pcm_writei(pcm_play_handle, data_out + offset, framesize);
            if((pcmreturn < 0) && (pcmreturn != -11))	// also ignore "temporarily unavailable" errors
            {
                // Handle an error condition from the snd_pcm_writei function
//			printf("Play PCM Write Error: %s  count = %d\n",snd_strerror(pcmreturn), play_write_error++);
                snd_pcm_prepare(pcm_play_handle);
            }
		
            if(pcmreturn >= 0)
            {
                // Calculate remaining number of samples to be sent and new position in sample array.
                // If all the samples were processed by the snd_pcm_writei function then framesize will be
                // zero and the while() loop will end.
                framesize -= pcmreturn;
                offset += (pcmreturn * 2);
            }
        }
        // End of new pcm play write routine


        //decimate the line out to half, ie from 96000 to 48000
        //play the received data (from left channel) to both of line out
		
        int jj = 0;
        int ii = 0;
        while (ii < ret_card){
            line_out[jj++] = output_i[ii] / LOOPBACK_LEVEL_DIVISOR;  // Left Channel. Reduce audio level to FLDIGI a bit
            line_out[jj++] = output_i[ii] / LOOPBACK_LEVEL_DIVISOR;  // Right Channel. Note: FLDIGI does not use the this channel.
            // The right channel can be used to output other integer values such as AGC, for capture by an
            // application such as audacity.
            ii += 2;	// Skip a pair of samples to account for the 96K sample to 48K sample rate change.
        }

/*
// This is the original pcm loopback write routine, now commented out.
while((pcmreturn = snd_pcm_writei(loopback_play_handle,
line_out, jj)) < 0){
//printf("loopback rx error: %s\n", snd_strerror(pcmreturn));
snd_pcm_prepare(loopback_play_handle);
//puts("preparing loopback");
}
*/    

        // This is the new pcm loopback write routine
        framesize = ret_card / 2;		// only writing half the number of samples because of the slower channel rate
        offset = 0;

        while(framesize > 0)
        {
            pcmreturn = snd_pcm_writei(loopback_play_handle, line_out + offset, framesize);
            if(pcmreturn < 0)
            {
//			printf("Loopback PCM Write Error: %s  count = %d\n",snd_strerror(pcmreturn), loopback_write_error++);
                // Handle an error condition from the snd_pcm_writei function
                snd_pcm_prepare(loopback_play_handle);
            }
		
            if(pcmreturn >= 0)
            {
                // Calculate remaining number of samples to be sent and new position in sample array.
                // If all the samples were processed by the snd_pcm_writei function then framesize will be
                // zero and the while() loop will end.
                framesize -= pcmreturn;
                offset += (pcmreturn * 2);
            }
        }
        // End of new pcm loopback write routine
	
    
		//played_samples += pcmreturn;
    }
	//fclose(pf);
    printf("********Ending sound thread\n");
}


void loopback_loop(){
	int32_t *data_in;
    int pcmreturn, j;

    int sample_size = snd_pcm_format_width(format) / 8;
    uint32_t buffer_size = loopback_period_size * sample_size * channels;
    data_in = (int32_t *)malloc(buffer_size);

    snd_pcm_prepare(loopback_capture_handle);

  while(sound_thread_continue) {

		//restart the pcm capture if there is an error reading the samples
		//this is opened as a blocking device, hence we derive accurate timing 

		while ((pcmreturn = snd_pcm_readi(loopback_capture_handle, data_in, loopback_period_size)) < 0){
			snd_pcm_prepare(loopback_capture_handle);
			//putchar('=');
		}
		j = 0;

		//fill up a local buffer, take only the left channel	
		j = 0;
		for (int i = 0; i < pcmreturn; i++){
			q_write(&qloop, data_in[j]/64);
			q_write(&qloop, data_in[j]/64);
			j += 2;
		}
		nsamples += j;

  }
  printf("********Ending loopback thread\n");
}

/*
We process the sound in a background thread.
It will call the user-supplied function sound_process()  
*/
void *sound_thread_function(void *ptr){
	char *device = (char *)ptr;
	struct sched_param sch;

	//switch to maximum priority
	sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(sound_thread, SCHED_FIFO, &sch);
	
	int i = 0;
	for (i = 0; i < 10; i++){
		if (!sound_start_play(device))
			break;
        delay(1000); //wait for the sound system to bootup
        printf("Retrying the sound system %d\n", i);
	}
	if (i == 10){
	 fprintf(stderr, "*Error opening play device");
		return NULL;
	}

	if (sound_start_capture(device)){
		fprintf(stderr, "*Error opening capture device");
		return NULL;
	}

//  printf("opening loopback on plughw:1,0 sound card\n");	
	if(sound_start_loopback_play("hw:1,0")){
		fprintf(stderr, "*Error opening loopback play device");
		return NULL;
	}
	sound_thread_continue = 1;
	sound_loop();
	sound_stop();

    return NULL;
}

void *loopback_thread_function(void *ptr){
	struct sched_param sch;

	//switch to maximum priority
	sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(loopback_thread, SCHED_FIFO, &sch);
//	printf("loopback thread is %x\n", loopback_thread);
//  printf("opening loopback on plughw:1,0 sound card\n");	

	if (sound_start_loopback_capture("hw:2,1")){
		fprintf(stderr, "*Error opening loopback capture device");
		return NULL;
	}
	sound_thread_continue = 1;
	loopback_loop();
	sound_stop();
    return NULL;
}

void sound_thread_start(char *device){
	q_init(&qloop, 10240);
 	qloop.stall = 1;

    pthread_barrier_init(&barrier, NULL, 2);

	pthread_create( &sound_thread, NULL, sound_thread_function, (void*)device);
    // wait for the sound_thread to come up
	pthread_barrier_wait(&barrier);

	pthread_create( &loopback_thread, NULL, loopback_thread_function, (void*)device);
}

void sound_thread_stop(){
	sound_thread_continue = 0;
}

void sound_input(int loop){
    if (loop){
        use_virtual_cable = 1;
	}
    else{
        use_virtual_cable = 0;
	}
}

//demo, uncomment it to test it out
/*
void sound_process(int32_t *input_i, int32_t *input_q, int32_t *output_i, int32_t *output_q, int n_samples){
 
	for (int i= 0; i < n_samples; i++){
		output_i[i] = input_i[i];
		output_q[i] = input_q[i];
	}	
}

void main(int argc, char **argv){
	sound_thread_start("plughw:0,0");
	sleep(10);
	sound_thread_stop();
	sleep(10);
}
*/
