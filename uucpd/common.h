/* Rhizo-UUCP
 * Copyright (C) 2018 Rhizomatica
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
 * Common functions used by any type of modem
 */

/**
 * @file common.h
 * @author Rafael Diniz
 * @date 09 May 2018
 * @brief Common functions used by any type of modem
 *
 * Common functions used by any type of modem.
 *
 */

#ifndef HAVE_COMMON__
#define HAVE_COMMON__

#include <stdint.h>
#include <pthread.h>

#include "uuardopd.h"

#ifdef __cplusplus
extern "C" {
#endif

bool inotify_wait(char *file_name);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_COMMON */
