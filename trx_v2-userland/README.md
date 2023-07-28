# Userland For HERMES Radio Transceiver Version 2

This repository contains the userland utils for the sBitx v2
radio for the HERMES system, which contains all the sBitx's control
code, and also the audio I/O management, which address the radio I/O
and loopback devices I/O.

## Compile And Install

To compile the project, run "make", and to install, run "make install".


## Userland Details

CW mode just generates a tone. GPS-based calibration is not supported by the radio (as it does not need).
This code uses kernel I2C interface, so enable it using the appropriate sBitx pinout and place this to
/boot/config.txt:

```
dtoverlay=i2c-gpio,i2c_gpio_delay_us=10,bus=2,i2c_gpio_sda=13,i2c_gpio_scl=6
```

Or alternativelly, in case the RTC is to be used as time reference to the Linux:

```
dtoverlay=i2c-rtc-gpio,ds1307,i2c_gpio_delay_us=10,bus=2,i2c_gpio_sda=13,i2c_gpio_scl=6
```

And remove all lines starting with "gpio=".

And add i2c-dev module to be loaded at boot (as root):

* echo i2c-dev >> /etc/modules

It is also strongly recommended to use the loopback locked to the wm8731. Add the following
parameter to snd-aloop module:

```
timer_source=hw:0,0
```

This code is designed to run on ARM64 (aarch64) Linux architecture.

## sbitx_client commands

Syntax:
* sbitx_client -c command [-a argument]

Examples:
* sbitx_client -c set_frequency -a 7100000
* sbitx_client -c get_frequency

Some commands need the argument parameter (-a), while some don't. Following is a
list of all commands provided by the sbitx_client. The commands are followed
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
  * Resp: USB | LSB | CW | ERROR

* set_mode
  * LSB | USB | CW
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
  * Resp: NO_GPS | ERROR

* restore_radio_defaults
  * No Argument
  * Resp: OK (restore default settings) | ERROR

* get_status
  * No Argument
  * Resp: 3 lines output with a constant word and a response:
  * PPS_STATUS FAIL
  * OFFSET_CAL_STATUS FAIL
  * LATEST_OFFSET_CAL Offset

* radio_reset
  * No Argument
  * Resp: OK (and the sbitx_controller exits immediately)

## sbitx_controller commands

The "-a" parameter disables all ALSA code, and "-h" shows the help.

Example:
* sbitx_controller

## Author

Rafael Diniz <rafael@rhizomatica.org>

## License

GPLv3
