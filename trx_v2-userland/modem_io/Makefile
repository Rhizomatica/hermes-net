CC = gcc
CFLAGS = -Wall -O2 -std=gnu11 -pthread -D_GNU_SOURCE -I../src

all: os_interop.o ring_buffer_posix.o shm_posix.o

os_interop.o: os_interop.c
	$(CC) $(CFLAGS) -c os_interop.c

ring_buffer_posix.o: ring_buffer_posix.c
	$(CC) $(CFLAGS) -c ring_buffer_posix.c

shm_posix.o: shm_posix.c
	$(CC) $(CFLAGS) -c shm_posix.c

.PHONY: all clean

clean:
	rm -f *.o
