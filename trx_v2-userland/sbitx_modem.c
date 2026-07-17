/* Sbitx Controller daemon - modem I/O
 *
 * Copyright (C) 2023-2025 Rhizomatica
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "sbitx_core.h"
#include "shm_utils.h"
#include "sbitx_modem.h"
#include "ring_buffer_posix.h"

cbuf_handle_t capture_buffer;
cbuf_handle_t playback_buffer;

int modem_create_shm()
{
    capture_buffer = NULL;
    playback_buffer = NULL;

    capture_buffer = circular_buf_init_shm(SIGNAL_BUFFER_SIZE, SIGNAL_INPUT);
    if (!capture_buffer) {
        fprintf(stderr, "modem_create_shm: failed to create capture shared buffer\n");
        return -1;
    }

    playback_buffer = circular_buf_init_shm(SIGNAL_BUFFER_SIZE, SIGNAL_OUTPUT);
    if (!playback_buffer) {
        fprintf(stderr, "modem_create_shm: failed to create playback shared buffer, cleaning up capture buffer\n");
        modem_destroy_shm();
        return -1;
    }

    return 0;
}

int modem_destroy_shm()
{
    if (capture_buffer) {
        if (capture_buffer->internal && capture_buffer->buffer) {
            circular_buf_destroy_shm(capture_buffer, SIGNAL_BUFFER_SIZE, SIGNAL_INPUT);
        }
        circular_buf_free_shm(capture_buffer);
        capture_buffer = NULL;
    }

    if (playback_buffer) {
        if (playback_buffer->internal && playback_buffer->buffer) {
            circular_buf_destroy_shm(playback_buffer, SIGNAL_BUFFER_SIZE, SIGNAL_OUTPUT);
        }
        circular_buf_free_shm(playback_buffer);
        playback_buffer = NULL;
    }

    return 0;
}
