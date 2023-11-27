CC=gcc
CFLAGS=-g -Wall -std=gnu11
LDFLAGS=-lwiringPi -li2c

.PHONY: clean

all: sbitx_controller

sbitx_controller: sbitx_i2c.o sbitx_core.o sbitx_gpio.o sbitx_si5351.o radio.o
	$(CC) -o sbitx_controller sbitx_i2c.o sbitx_core.o sbitx_gpio.o sbitx_si5351.o radio.o $(LDFLAGS)

radio.o: radio.c
	$(CC) -c $(CFLAGS) radio.c -o radio.o


sbitx_gpio.o: sbitx_gpio.c sbitx_gpio.h
	$(CC) -c $(CFLAGS) sbitx_gpio.c -o sbitx_gpio.o

sbitx_i2c.o: sbitx_i2c.c sbitx_i2c.h
	$(CC) -c $(CFLAGS) sbitx_i2c.c -o sbitx_i2c.o

sbitx_core.o: sbitx_core.c sbitx_core.h
	$(CC) -c $(CFLAGS) sbitx_core.c -o sbitx_core.o

sbitx_si5351.o: sbitx_si5351.c sbitx_si5351.h
	$(CC) -c $(CFLAGS) sbitx_si5351.c -o sbitx_si5351.o



clean:
	rm -f sbitx_controller *.o
