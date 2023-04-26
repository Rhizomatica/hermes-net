prefix=/usr
CC=gcc

CFLAGS=`pkg-config --cflags gtk+-3.0` -Wall -pedantic -std=gnu11 -fstack-protector
LDFLAGS=`pkg-config --libs gtk+-3.0` -lwiringPi -lasound -lm -lfftw3 -lfftw3f -pthread -lncurses -lsqlite3

all: sbitx

SOURCES=vfo.c si570.c sbitx_sound.c fft_filter.c sbitx_gtk.c sbitx_utils.c i2cbb.c si5351v2.c ini.c queue.c logbook.c telnet.c macros.c remote.c mongoose.c webserver.c sbitx.c

OBJ=$(SOURCES:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

sbitx: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean install

install: sbitx
	mkdir -p /home/pi/sbitx/audio
	mkdir -p /home/pi/sbitx/data
	mkdir -p /home/pi/sbitx/web
	sqlite3 /home/pi/sbitx/data/sbitx.db < data/create_db.sql

clean:
	rm -rf sbitx *.o
