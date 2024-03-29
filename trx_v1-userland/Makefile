# uBitxv6 Rhizomatica tools
# Copyright (C) 2019-2022 Rhizomatica
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
CFLAGS=-Wall -pedantic -std=gnu11 -pthread -fstack-protector

all: ubitx_controller ubitx_client

ubitx_controller: ubitx_controller.c serial.c shm.c ubitx_controller.h serial.h shm.h ../include/radio_cmds.h
	$(CC) $(CFLAGS) ubitx_controller.c serial.c shm.c -o ubitx_controller

ubitx_client: ubitx_client.c shm.c ubitx_io.c ubitx_controller.h ubitx_io.h shm.h help.h ../include/radio_cmds.h
	$(CC) $(CFLAGS) ubitx_client.c ubitx_io.c shm.c -o ubitx_client

.PHONY: clean install
clean:
	rm -f *.o ubitx_client ubitx_controller

install: ubitx_controller ubitx_client
	install -D ubitx_client $(DESTDIR)$(prefix)/bin/ubitx_client
	install -D ubitx_controller $(DESTDIR)$(prefix)/bin/ubitx_controller
	install -m 644 -D ../system_services/init/ubitx.service $(DESTDIR)/etc/systemd/system/ubitx.service
	install -m 644 -D ../system_services/udev/99-radio.rules $(DESTDIR)/etc/udev/rules.d/99-radio.rules
