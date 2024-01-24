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

#ifndef SBITX_ALSA_H_
#define SBITX_ALSA_H_

#include <alsa/asoundlib.h>

#include "sbitx_core.h"

void sound_system_init(radio *radio_h, pthread_t *control_tid, pthread_t *radio_capture,
                       pthread_t *radio_playback, pthread_t *loop_capture, pthread_t *loop_playback);

void sound_system_shutdown(radio *radio_h, pthread_t *control_tid, pthread_t *radio_capture,
                           pthread_t *radio_playback, pthread_t *loop_capture, pthread_t *loop_playback);


void show_alsa(snd_pcm_t *handle, snd_pcm_hw_params_t *params);
void sound_mixer(char *card_name, char *element, int make_on);
void clear_buffers();

#endif
