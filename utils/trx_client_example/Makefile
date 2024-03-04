# sBitx interface sample
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


prefix=/usr
CC=gcc
CPP=g++
CFLAGS=-Ofast -Wall -pedantic -pthread -Wno-deprecated-declarations -std=gnu11
CPPFLAGS=-Ofast -Wall -pedantic -Wno-format -pthread -std=gnu++14
LDFLAGS=-pthread

all: trx_client_example

# compile the main.cpp and link using C++
trx_client_example: shm_utils.o sbitx_io.o main.o
	$(CPP) $(LDFLAGS) -o trx_client_example shm_utils.o sbitx_io.o main.o

main.o: main.cpp
	$(CPP) -c $(CPPFLAGS) main.cpp -o main.o


# the shared memory interface itself is implemented in C
sbitx_io.o: sbitx_io.c sbitx_io.h
	$(CC) -c $(CFLAGS) sbitx_io.c -o sbitx_io.o

shm_utils.o: shm_utils.c shm_utils.h
	$(CC) -c $(CFLAGS) shm_utils.c -o shm_utils.o


.PHONY: clean

clean:
	rm -rf trx_client_example *.o
