# sBitx controller for HERMES
#
# Copyright (C) 2024 Rhizomatica
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

CC=gcc
LDFLAGS=-liniparser -li2c -lssl -lcrypto -lpthread
CFLAGS=-Ofast -Wall -std=gnu11 -fstack-protector -I/usr/include/iniparser

ifeq (${uname_p},aarch64)
# aarch64 Raspberry Pi 4 or better
	CFLAGS+=-moutline-atomics -march=armv8-a+crc -I/usr/include/iniparser
# for Pi 5 use:
#	CFLAGS=-march=armv8.2-a+crypto+fp16+rcpc+dotprod
else
# x86_64 with SSE 4.2 level or better
	CFLAGS+=-march=x86-64-v2
endif

GPIOLIB_OBJS=gpiolib/gpiolib.o gpiolib/gpiochip_bcm2712.o gpiolib/gpiochip_bcm2835.o gpiolib/gpiochip_rp1.o gpiolib/util.o

.PHONY: clean

all: sbitx_controller

sbitx_controller: sbitx_i2c.o sbitx_core.o sbitx_gpio.o sbitx_si5351.o sbitx_websocket.o cfg_utils.o mongoose.o sbitx_controller.o $(GPIOLIB_OBJS)
	$(CC) -o sbitx_controller sbitx_i2c.o sbitx_core.o sbitx_gpio.o sbitx_si5351.o sbitx_websocket.o cfg_utils.o mongoose.o sbitx_controller.o $(GPIOLIB_OBJS) $(LDFLAGS)

sbitx_controller.o: sbitx_controller.c
	$(CC) -c $(CFLAGS) sbitx_controller.c -o sbitx_controller.o

cfg_utils.o: cfg_utils.c cfg_utils.h
	$(CC) -c $(CFLAGS) cfg_utils.c -o cfg_utils.o

sbitx_gpio.o: sbitx_gpio.c sbitx_gpio.h
	$(CC) -c $(CFLAGS) sbitx_gpio.c -o sbitx_gpio.o

sbitx_i2c.o: sbitx_i2c.c sbitx_i2c.h
	$(CC) -c $(CFLAGS) sbitx_i2c.c -o sbitx_i2c.o

sbitx_core.o: sbitx_core.c sbitx_core.h
	$(CC) -c $(CFLAGS) sbitx_core.c -o sbitx_core.o

sbitx_si5351.o: sbitx_si5351.c sbitx_si5351.h
	$(CC) -c $(CFLAGS) sbitx_si5351.c -o sbitx_si5351.o

# websocket stuff
sbitx_websocket.o: sbitx_websocket.c sbitx_websocket.h
	$(CC) -c $(CFLAGS) -Wno-missing-braces sbitx_websocket.c -o sbitx_websocket.o

mongoose.o: mongoose.c mongoose.h
	$(CC) -c $(CFLAGS) -DMG_ENABLE_OPENSSL -DMG_TLS=MG_TLS_OPENSSL -DMG_ARCH=1 mongoose.c -o mongoose.o

# gpiolib stuff
gpiolib/gpiolib.o: gpiolib/gpiolib.c gpiolib/gpiolib.h
	$(CC) -c $(CFLAGS) gpiolib/gpiolib.c -o gpiolib/gpiolib.o

gpiolib/gpiochip_bcm2712.o: gpiolib/gpiochip_bcm2712.c gpiolib/gpiochip.h
	$(CC) -c $(CFLAGS) gpiolib/gpiochip_bcm2712.c -o gpiolib/gpiochip_bcm2712.o

gpiolib/gpiochip_bcm2835.o: gpiolib/gpiochip_bcm2835.c gpiolib/gpiochip.h
	$(CC) -c $(CFLAGS) gpiolib/gpiochip_bcm2835.c -o gpiolib/gpiochip_bcm2835.o

gpiolib/gpiochip_rp1.o:  gpiolib/gpiochip_rp1.c gpiolib/gpiochip.h
	$(CC) -c $(CFLAGS) gpiolib/gpiochip_rp1.c -o gpiolib/gpiochip_rp1.o

gpiolib/util.o: gpiolib/util.c gpiolib/util.h
	$(CC) -c $(CFLAGS) gpiolib/util.c -o gpiolib/util.o

install: sbitx_controller
	install -D sbitx_controller $(DESTDIR)$(prefix)/bin/sbitx_controller

install_new:
	install -D config/core.ini $(DESTDIR)/etc/sbitx/core.ini
	install -D config/user.ini $(DESTDIR)/etc/sbitx/user.ini
	mkdir -p /etc/sbitx/web
	install -D web/* /etc/sbitx/web/

clean:
	rm -f sbitx_controller *.o gpiolib/*.o
