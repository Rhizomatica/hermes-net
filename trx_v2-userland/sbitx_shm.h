/* ubitx_controller
 * Copyright (C) 2021-2022 Rhizomatica
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

#ifndef HAVE_SBITX_SHM_H__
#define HAVE_SBITX_SHM_H__

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

#include "sbitx_core.h"

void shm_controller_init(radio *radio_h, pthread_t *shm_tid);
void shm_controller_shutdown(pthread_t *shm_tid);


#endif // HAVE_SBITX_SHM_H__
