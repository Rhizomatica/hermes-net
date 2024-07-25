/* HERMES radio
 *
 * Copyright (C) 2024 Rhizomatica
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

#include <threads.h>

#include "sbitx_external_dsp.h"

mtx_t mtx;
cnd_t cnd;

int external_dsp_init()
{
    mtx_init(&mtx, mtx_plain);
    cnd_init(&cnd);

    if (radio_h->profiles[radio_h_snd->profile_active_idx].operating_mode == OPERATING_MODE_EXTERNAL_DSP)
        radio_h->internal_dsp_off = true;
    else
        radio_h->internal_dsp_off = false;
}

int external_dsp_close()
{
    mtx_destroy(&mtx);
    cnd_destroy(&cnd);

}

// just waits until external dsp is disabled
int check_external_dsp(radio *radio_h)
{
    if(mtx_lock(&mtx) != thrd_success)
        return -1;

    if (radio_h->profiles[radio_h_snd->profile_active_idx].operating_mode == OPERATING_MODE_EXTERNAL_DSP)
    {
        radio_h->internal_dsp_off = true;
        if(cnd_wait(&cnd, &mtx) != thrd_success)
            return -1;
    }
    mtx_unlock(&mtx);
    radio_h->internal_dsp_off = false;
}

int disable_external_dsp(radio *radio_h)
{
    while (radio_h->internal_dsp_off == true)
        cnd_signal(&cnd);
}
