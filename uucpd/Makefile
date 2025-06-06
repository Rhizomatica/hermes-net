# UUCPD: UUCP <-> HF tools
# Copyright (C) 2019-2025 Rhizomatica
# Author: Rafael Diniz <rafael@riseup.net>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

uname_p := $(shell uname -m)

prefix=/usr
CC=gcc
CFLAGS=-O3 -Wall -pedantic -std=gnu11 -pthread -fstack-protector -I../include

ifeq (${uname_p},aarch64)
# aarch64 Raspberry Pi 4 or better
	CFLAGS+=-moutline-atomics -march=armv8-a+crc
# for Pi 5 use:
#	CFLAGS+=-march=armv8.2-a+crypto+fp16+rcpc+dotprod
else
# x86_64 with SSE 4.2 level or better
	CFLAGS+=-march=x86-64-v2
endif

ifeq ($(IS_SBITX), 0)
	EXTRA_OBJ=../trx_v1-userland/ubitx_io.o
else
	EXTRA_OBJ=../trx_v2-userland/sbitx_io.o
endif

all: uucpd uuport

%.o : %.c %.h
	gcc -c $(CFLAGS) $< -o $@

uucpd: ardop.o shm.o net.o circular_buffer.o uucpd.o call_uucico.o serial.o vara.o $(EXTRA_OBJ)
	$(CC) $(CFLAGS) -o $@ $^


uuport: uuport.o shm.o circular_buffer.o
	$(CC) $(CFLAGS) -o $@ $^


.PHONY: clean install doc
install: uucpd uuport
	install -D uucpd $(DESTDIR)$(prefix)/bin/uucpd
	install -D uuport $(DESTDIR)$(prefix)/bin/uuport

doc:
	doxygen doxyfile

clean:
	rm -rf uucpd uuport *.o $(EXTRA_OBJ) *~ doc
