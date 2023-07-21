/*
    sBitx controller
    Copyright (C) 2015-2023 Rafael Diniz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HAVE_BUFFER_H__
#define HAVE_BUFFER_H__

#include <stdint.h>
#include <pthread.h>

#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    pthread_mutex_t    mutex;
    pthread_cond_t     cond;
    struct ring_buffer buf;
} buffer;

extern void read_buffer(buffer *buf_in, uint8_t *buffer_out, int size);
extern void write_buffer(buffer *buf_out, uint8_t *buffer_in, int size);
void initialize_buffer(buffer *buf, int mag);
void initialize_buffers();
void clear_buffer (buffer *buffer);


extern buffer *radio_to_dsp;
extern buffer *dsp_to_radio;

extern buffer *mic_to_dsp;
extern buffer *dsp_to_speaker;

extern buffer *dsp_to_loopback;
extern buffer *loopback_to_dsp;

#ifdef __cplusplus
};
#endif

#endif /* HAVE_BUFFER__ */
