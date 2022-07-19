# UUXCOMP
# Copyright (C) 2022 Rhizomatica
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
CFLAGS=-g -Wall -pedantic -Wno-format-overflow -std=gnu11 -pthread -fstack-protector
LDFLAGS=-lb64 -llzma -lz

all: uuxcomp crmail

uuxcomp: uuxcomp.c xz_compression.c xz_compression.h gz_compress.c gz_compress.h
	$(CC) $(CFLAGS) uuxcomp.c xz_compression.c gz_compress.c -o uuxcomp $(LDFLAGS)

crmail: crmail.c xz_decompression.c xz_decompression.h gz_compress.c gz_compress.h
	$(CC) $(CFLAGS) crmail.c xz_decompression.c gz_compress.c -o crmail $(LDFLAGS)

.PHONY: clean install
clean:
	rm -f *.o uuxcomp crmail

install: uuxcomp crmail
	install -D uuxcomp $(DESTDIR)$(prefix)/bin/uuxcomp
	install -D crmail $(DESTDIR)$(prefix)/bin/crmail
