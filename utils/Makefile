# Assorted utils
# Copyright (C) 2019-2023 Rhizomatica
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


prefix=/usr
CC=gcc
CFLAGS=-g -Wall -pedantic -std=gnu11 -pthread -fstack-protector -Wno-deprecated-declarations
LDFLAGS=-lm -lasound

all: keyon keyoff alsa-capture alsa-show alsa-play i2c_test

%.o : %.c %.h
	gcc -c $(CFLAGS) $< -o $@

keyon: keyon.o serial.o
	$(CC) $(CFLAGS) -o $@ $^

keyoff: keyoff.o serial.o
	$(CC) $(CFLAGS) -o $@ $^

alsa-capture: alsa-capture.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

alsa-show: alsa-show.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

alsa-play: alsa-play.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

i2c_test: i2c_test.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -li2c

.PHONY: clean

clean:
	rm -rf keyon keyoff *.o
