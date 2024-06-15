# Arduino Make file. Refer to https://github.com/sudar/Arduino-Makefile

#CFLAGS=-std=c17

# Set this to a recent Arduino release
ARDUINO_DIR = /usr/share/arduino/
#ARDUINO_DIR = /home/rafael2k/files/rhizomatica/hermes/arduino-1.8.13
AVR_TOOLS_DIR = /

ALTERNATE_CORE_PATH = ./ATTinyCore/avr
#ALTERNATE_CORE_PATH = ./ATTinyCore/tiny

USER_LIB_PATH = ./ATTinyCore/avr/libraries
LIBRARY := Wire

ALTERNATE_CORE = tiny
BOARD_TAG = attinyx5
BOARD_SUB = 85
MCU=attiny85
F_CPU = 1000000L
ISP_PROG = usbtiny


#BOARD_TAG = attiny85at8
#ALTERNATE_CORE = tiny
#F_CPU = 1000000L
#ISP_PROG = usbtiny


include /usr/share/arduino/Arduino.mk
# !!! Important. You have to use make ispload to upload when using ISP programmer
