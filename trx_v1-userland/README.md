# Userland For HERMES Radio Transceiver Version 1

This repository contains the userland utils for the uBitx v6
based Rhizomatica's HF radio transceiver version 1.

## Compile And Install

To compile the project, run "make", and to install, run "make install".


## Userland Details

## ubitx_client commands

Syntax:
* ubitx_client -c command [-a argument]

Examples:
* ubitx_client -c set_frequency -a 7100000
* ubitx_client -c get_frequency

Some commands need the argument parameter (-a), while some don't. Following is a
list of all commands provided by the ubitx_client. The commands are followed
by the argument type and possible responses.

* ptt_on
  * No Argument
  * Resp: OK | NOK | SWR | ERROR

* ptt_off
  * No Argument
  * Resp: OK | NOK | SWR | ERROR

* get_frequency
  * No Argument
  * Resp: Frequency | ERROR

* set_frequency
  * Frequency
  * Resp: OK | ERROR

* get_mode
  * No Argument
  * Resp: USB | LSB | ERROR

* set_mode
  * LSB | USB
  * Resp: OK | ERROR

* get_txrx_status
  * No Argument
  * Resp: INTX | INRX | ERROR

* get_protection_status
  * No Argument
  * Resp: PROTECTION_ON | PROTECTION_OFF | ERROR

* get_mastercal
  * No Argument
  * Resp: Frequency | ERROR

* set_mastercal
  * Frequency
  * Resp: OK | ERROR

* get_bfo
  * No Argument
  * Resp: Frequency | ERROR

* set_bfo
  * Frequency
  * Resp: OK | ERROR

* get_fwd
  * No Argument
  * Resp: Power | ERROR

* get_ref
  * No Argument
  * Resp: Power | ERROR

* get_led_status
  * No Argument
  * Resp: LED_ON | LED_OFF | ERROR

* set_led_status
  * 0 | 1
  * Resp: OK | ERROR

* get_connection_status
  * No Argument
  * Resp: LED_ON | LED_OFF | ERROR

* set_connection_status
  * 0 | 1
  * Resp: OK | ERROR

* get_serial
  * No Argument
  * Resp: Serial Number | ERROR

* set_serial
  * Serial Number
  * Resp: OK | ERROR

* reset_protection
  * No Argument
  * Resp: OK | ERROR

* set_ref_threshold
  * Reflected Threshold Level For Protection Activation (0 - 1023)
  * Resp: OK | ERROR

* get_ref_threshold
  * No Argument
  * Resp: Reflected Threshold Level | ERROR

* set_radio_defaults
  * No Argument
  * Resp: OK (set default settings) | ERROR

* gps_calibrate
  * No Argument
  * Resp: OK (start 10s GPS-based calibration procedure) | NO_GPS | ERROR

* restore_radio_defaults
  * No Argument
  * Resp: OK (restore default settings) | ERROR

* get_status
  * No Argument
  * Resp: 3 lines output with a constant word and a response:
  * PPS_STATUS OK | FAIL
  * OFFSET_CAL_STATUS OK | FAIL
  * LATEST_OFFSET_CAL Offset

* radio_reset
  * No Argument
  * Resp: OK (and the ubitx_controller exits immediately)

## ubitx_controller commands

Syntax:
* ubitx_controller -s serial_device -r [icom,ubitx]

Examples:
* ubitx_client -c /dev/ttyUSB0 -r ubitx

## Author

Rafael Diniz <rafael@rhizomatica.org>

## License

GPLv3
