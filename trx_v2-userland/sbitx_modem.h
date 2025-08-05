#ifndef MODEM_SBITX_H
#define MODEM_SBITX_H

// audio buffers shared memory interface
// 1536000 * 8
#define SIGNAL_BUFFER_SIZE 12288000
#define SIGNAL_OUTPUT "/signal-radio2modem"
#define SIGNAL_INPUT "/signal-modem2radio"

#define MODEM_IO_ALSA 0
#define MODEM_IO_SHM 1

#include "ring_buffer_posix.h"

extern cbuf_handle_t capture_buffer;
extern cbuf_handle_t playback_buffer;


int modem_create_shm();

int modem_destroy_shm();

#endif // MODEM_SBITX_H
