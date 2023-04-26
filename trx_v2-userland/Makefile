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
CFLAGS=-Wall -pedantic -std=gnu11 -pthread -fstack-protector

all: sbitx_controller sbitx_client

sbitx_controller: sbitx_controller.c shm.c sbitx_controller.h shm.h ../include/radio_cmds.h
	$(CC) $(CFLAGS) sbitx_controller.c shm.c -o sbitx_controller

sbitx_client: sbitx_client.c shm.c sbitx_io.c sbitx_controller.h sbitx_io.h shm.h help.h ../include/radio_cmds.h
	$(CC) $(CFLAGS) sbitx_client.c sbitx_io.c shm.c -o sbitx_client

.PHONY: clean install
clean:
	rm -f *.o sbitx_client sbitx_controller

install: sbitx_controller sbitx_client
	install -D sbitx_client $(DESTDIR)$(prefix)/bin/sbitx_client
	install -D sbitx_controller $(DESTDIR)$(prefix)/bin/sbitx_controller
	install -m 644 -D ../system_services/init/sbitx.service $(DESTDIR)/etc/systemd/system/sbitx.service
