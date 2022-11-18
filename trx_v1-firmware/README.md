# Firmware For HERMES Radio Transceiver Version 1

This repository contains the firmware and userland tools for the uBitx v6
based Rhizomatica's HF radio transceiver version 1. There are three variants
of the radio, two with GPS corrected PLL, and another without GPS. We can RADUINO_VER 0 the
original uBitx v6 Raduino with minor modifications and without GPS. RADUINO_VER 1 is the one
with the modifies uBitx v6 with GPS. RADUINO_VER 2 is the Rhizomatica's Raduino.

## Compile And Install

To compile the project, run "make". To install the firmware, run "make
ispload" (make sure ubitx service is stopped).

## Variants information

### Firmware Details For Variant RADUINO_VER == 0

Ubitx v6 connector pin assignments:

*      Pin 1 (Violet), A7, REF MEASURE input
*      Pin 2 (Blue),   A6, FWD MEASURE input
*      Pin 3 (Green), +5v
*      Pin 4 (Yellow), GND
*      Pin 5 (Orange), A3, SYSTEM LED output
*      Pin 6 (Red),    A2, CONNECTED LED output
*      Pin 7 (Brown),  A1, ANT HIGH SWR RED LED output
*      Pin 8 (Black),  A0, ANT GOOD GREEN LED output

### Firmware Details For Variant RADUINO_VER == 1

Ubitx v6 connector pin assignments:

*      Pin 1 (Violet), A7, REF MEASURE input
*      Pin 2 (Blue),   A6, FWD MEASURE input
*      Pin 3 (Green), +5v
*      Pin 4 (Yellow), GND
*      Pin 5 (Orange), A3, SYSTEM LED output
*      Pin 6 (Red),    A2, CONNECTED LED output
*      Pin 7 (Brown),  A1, ANT HIGH SWR RED LED output
*      Pin 8 (Black),  A0, ANT GOOD GREEN LED output

D5 and D2 with changes for GPS calibration! LPF_A and CW_KEY connection to Arduino re-routed. New
Arduino pin assignment, as follows:

*      D12, TX_LPF_A,      LPF_A (re-routed!)
*      D11, CW_KEY,        Pin goes high during CW keydown to transmit the carrier. (re-routed!)
*      D7, TX_RX,          Pin from the Nano to the radio to switch to TX (HIGH) and RX(LOW)
*      D6, CW_TONE,        Generates a square wave sidetone while sending the CW
*      D5, CAL_CLK,        CLK #0 connects here for calibration purposes (original TX_LPF_A disconnected - cut the trace in board)
*      D4, TX_LPF_B,       LPF_B
*      D3, TX_LPF_C,       LPF_C
*      D2, PPS_IN,         GPS 1PPS input  (original CW_KEY  disconnected - cut trace in board)

### Firmware Details For Variant RADUINO_VER == 2

Pin assignments for Rhizomatica's Raduino:

*   D7  TX_RX
*   D12 TX_LPF_A
*   D5  CAL_CLK
*   D4  TX_LPF_B
*   D3  TX_LPF_C
*   D11 CW_KEY
*   D2  PPS_IN
*   D8  ANT_GOOD
*   D9  ANT_HIGH_SWR
*   D10 LED_CONNECTED
*   D13 LED_CONTROL
*   D6  TX_LED
*   A0  VDC_IN
*   A6  ANALOG_FWD
*   A7  ANALOG_REF
*   A1  AUX1
*   A2  AUX2
*   A3  AUX3

## C compiler defines

   Set the firmware Makefile for different radio variants. Use the define HAS_GPS for
   the variant of the radio with GPS board inside, or leave without the define for the variant
   without GPS.

## Raduino Modifications

   For both variants, the resistor R2 needs to be removed. Also the pins 17 and 18 of the
   connector between the Raduino and the main uBitx board need to be cut (or cut the tracks 
   which connect to them).
   
   Also, for the GPS variant of the radio, please refer to the pictures available here: https://github.com/DigitalHERMES/rhizo-transceiver/raw/main/ubitxv6_mods/


## Author

Rafael Diniz <rafael@rhizomatica.org>

## License

GPLv3
