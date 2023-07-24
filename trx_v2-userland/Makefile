# sBitxv2 Rhizomatica tools
# Copyright (C) 2023 Rhizomatica
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

CC=gcc
prefix=/usr
# CFLAGS=-g -Wall -pedantic -std=gnu11 -pthread -fstack-protector
CFLAGS=`pkg-config --cflags gtk+-3.0` -O3 -Wall -Wno-deprecated-declarations -std=gnu11 -pthread -fstack-protector -DMG_ENABLE_OPENSSL -DMG_ARCH=1
LDFLAGS=`pkg-config --libs glib-2.0` -lwiringPi -lasound -lm -lfftw3 -lfftw3f -pthread -lncurses -lssl -lcrypto

all: sbitx_controller sbitx_client

# SOURCES=vfo.c sbitx_sound.c fft_filter.c sbitx_gtk.c i2cbb.c si5351v2.c ini.c queue.c mongoose.c webserver.c sbitx.c sbitx_alsa.c sbitx_controller.c shm.c
SOURCES=vfo.c fft_filter.c sbitx_gtk.c i2cbb.c si5351v2.c ini.c queue.c mongoose.c webserver.c sbitx.c sbitx_alsa.c sbitx_controller.c shm.c buffer.c ring_buffer.c

OBJ=$(SOURCES:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

sbitx_controller: $(OBJ) sbitx_controller.h shm.h ../include/radio_cmds.h
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

sbitx_client: sbitx_client.c shm.c sbitx_io.c sbitx_controller.h sbitx_io.h shm.h help.h ../include/radio_cmds.h
	$(CC) $(CFLAGS) sbitx_client.c sbitx_io.c shm.c -o sbitx_client

.PHONY: clean install
clean:
	rm -f *.o sbitx_client sbitx_controller

install: sbitx_controller sbitx_client
	install -D sbitx_client $(DESTDIR)$(prefix)/bin/sbitx_client
# ugly hack for backward compatibility
	install -D sbitx_client $(DESTDIR)$(prefix)/bin/ubitx_client
	install -D sbitx_controller $(DESTDIR)$(prefix)/bin/sbitx_controller
	mkdir -p /etc/sbitx/web
	install -D web/* /etc/sbitx/web/
	install -D data/user_settings.ini /etc/sbitx/user_settings.ini
	install -D data/hw_settings.ini /etc/sbitx/hw_settings.ini
	mkdir -p /etc/sbitx/audio
	install -m 644 -D ../system_services/init/sbitx.service $(DESTDIR)/etc/systemd/system/sbitx.service
