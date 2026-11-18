/* sbitx_codec - the sBitx WM8731's digital interface activated once both
 * of the controller's codec streams are up.
 *
 * Copyright (C) 2026 Rhizomatica
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * About one daemon start in five left the sBitx receiver deaf: the codec
 * delivered its noise but no signal, and stayed so until the next start.
 * Everything readable matched a healthy start, and on the bench a deaf
 * codec came back the moment its ACTIVE register (R9) was cleared and set
 * again, while rewriting every other register, the Si5351, the relays or
 * the ALSA stream left it deaf.  The likely reason, not proven: the driver
 * sets ACTIVE as a DAPM widget that powers up with the first stream, while
 * the codec's power-up sequence sets it after the rest is powered, and the
 * two streams are configured from two threads at once.  Clearing and
 * setting it once both streams are configured makes it the last step.
 */
#ifndef SBITX_CODEC_H_
#define SBITX_CODEC_H_

#include <stdbool.h>
#include <stdint.h>

/* Which capture channel is which, from their DC offsets: the codec runs
 * with its ADC high-pass off, and on the sBitx the microphone's input sits
 * well above the receiver's (bench: mic -15 dBFS, receiver -35).  RIGHT
 * when the microphone (right) leads by CODEC_ORDER_MARGIN_DB, SWAPPED when
 * the receiver does, UNKNOWN when the two are too close to tell. */
typedef enum { CODEC_ORDER_RIGHT, CODEC_ORDER_SWAPPED, CODEC_ORDER_UNKNOWN } codec_order;
codec_order sbitx_codec_order(double left_dc_dbfs, double right_dc_dbfs);

/* Reactivates the WM8731's interface until it comes back in the right
 * channel order: clears ACTIVE, waits CODEC_INACTIVE_MS, sets it again,
 * and checks the order (sound_codec_probe_dc, sbitx_codec_order).  On the
 * bench a reactivation lands swapped about three times in four, and every
 * one that landed in the right order heard (10 of 10); a start that was
 * never reactivated went deaf about one time in five, its order looking
 * right.  Nothing is cycled when the order can't be told on this board.
 * The codec is written over I2C directly (the driver cannot be asked to);
 * the value written back is the one the driver's cache holds, so its view
 * stays true.  False, logged, when the codec was not left in the right
 * order. */
bool sbitx_codec_reactivate(void);

/* The first device in devices_dir (the kernel's /sys/bus/i2c/devices, as
 * <bus>-<hex address> entries) whose name file says wm8731: its bus and
 * 7-bit address.  False when there is none. */
bool sbitx_codec_find(const char *devices_dir, int *bus, int *addr);

/* A WM8731 register write as it goes on the bus: 7-bit register, then the
 * 9-bit value's top bit, then its low byte. */
void sbitx_codec_encode(int reg, unsigned value, uint8_t out[2]);

#endif
