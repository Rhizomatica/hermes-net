/*
 *   sBitx controller
 *   Copyright (C) 2018-2024 Rafael Diniz
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

#include "sbitx_buffer.h"

buffer *radio_to_dsp;
buffer *dsp_to_radio;

buffer *mic_to_dsp;
buffer *dsp_to_speaker;

buffer *dsp_to_loopback;
buffer *loopback_to_dsp;

inline unsigned long size_buffer(buffer *buffer)
{
    return ring_buffer_count_bytes(&buffer->buf);
}

inline void read_buffer(buffer *buf_in, uint8_t *buffer_out, int size) {
    void *addr;

try_again_read:
    pthread_mutex_lock( &buf_in->mutex );
    if ( ring_buffer_count_bytes( &buf_in->buf ) >= size )
    {
        addr = ring_buffer_read_address( &buf_in->buf );
        memcpy( buffer_out, addr, size );
        ring_buffer_read_advance( &buf_in->buf, size );
        pthread_cond_signal( &buf_in->cond );
        pthread_mutex_unlock( &buf_in->mutex );
    }
    else
    {
        pthread_cond_wait( &buf_in->cond, &buf_in->mutex );
        pthread_mutex_unlock( &buf_in->mutex );
        goto try_again_read;
    }


}

inline void write_buffer(buffer *buf_out, uint8_t *buffer_in, int size) {
    void *addr;

try_again_write:
    pthread_mutex_lock( &buf_out->mutex );
    if ( ring_buffer_count_free_bytes ( &buf_out->buf ) >= size)
    {
        addr = ring_buffer_write_address( &buf_out->buf );
        memcpy( addr, buffer_in, size );
        ring_buffer_write_advance ( &buf_out->buf, size );
        pthread_cond_signal( &buf_out->cond );
        pthread_mutex_unlock( &buf_out->mutex );
    }
    else
    {
        pthread_cond_wait( &buf_out->cond, &buf_out->mutex );
        pthread_mutex_unlock( &buf_out->mutex );
        goto try_again_write;
    }
}

void initialize_buffer(buffer *buf, int mag) // size is 2^mag
{
    pthread_mutex_init( &buf->mutex, NULL );
    pthread_cond_init( &buf->cond, NULL );
    ring_buffer_create( &buf->buf, mag );
}

void clear_buffer (buffer *buffer)
{
    pthread_mutex_lock( &buffer->mutex );
    ring_buffer_clear(&buffer->buf);
    pthread_mutex_unlock( &buffer->mutex );

}

void initialize_buffers()
{
    static buffer radio_to_dsp_int;
    radio_to_dsp = &radio_to_dsp_int;
    static buffer dsp_to_radio_int;
    dsp_to_radio = &dsp_to_radio_int;

    static buffer mic_to_dsp_int;
    mic_to_dsp = &mic_to_dsp_int;
    static buffer dsp_to_speaker_int;
    dsp_to_speaker = &dsp_to_speaker_int;

    static buffer dsp_to_loopback_int;
    dsp_to_loopback = &dsp_to_loopback_int;
    static buffer loopback_to_dsp_int;
    loopback_to_dsp = &loopback_to_dsp_int;

    initialize_buffer(radio_to_dsp, 18);
    initialize_buffer(dsp_to_radio, 18);

    initialize_buffer(mic_to_dsp, 18);
    initialize_buffer(dsp_to_speaker, 18);

    initialize_buffer(dsp_to_loopback, 18);
    initialize_buffer(loopback_to_dsp, 18);
}
