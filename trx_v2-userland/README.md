# trx_v3-userland

Next generation of HERMES radio controller software for the sBitx v3. 

This software is aims to be a complete low-level implementation for all sBitx features, plus some DSP and ALSA code for providing a ready-to-use voice and digital functionality. Already present in the code are the following features:

* I2C communication to the Si5351a and ATTiny85 (power fwd and ref readings) using libi2c
* GPIO using the GPIOLIB library (gpiolib directory), for all kind of radio controls (encoders, lpf bank, tx/rx)
* Si5351a control functions
* Configuration files located in /etc/sbitx/{core,user}.ini
* Websocket streaming for Web UI

This code expects the I2C bus to be kernel I2C interface. Make sure you have one of the dtoverlays
in /boot/config.txt

For loading the I2C Bit-Banged (BB) driver in the appropriate sBitx pins, with kernel RTC clock driver loaded (for use as system clock, this is what I use):
```
dtoverlay=i2c-rtc-gpio,ds1307,bus=2,i2c_gpio_sda=13,i2c_gpio_scl=6
```
For just loading I2C bus:

```
dtoverlay=i2c-gpio,bus=2,i2c_gpio_sda=13,i2c_gpio_scl=6
```

And comment out all lines starting with "gpio=".

For detecting the I2C interfaces use:

```
i2cdetect -l

```

With gpio and RTC, it lists as:

```
i2c-22  i2c             i2c-gpio-rtc@0                          I2C adapter
```

By looking the I2C bus where the I2C BB driver is loaded, use the appropriate device file, eg., /dev/i2c-22 or /dev/i2c-2, in "/etc/sbitx/core.ini".

# Compilation

Just type "make" to build the two example applications. 


# Usage

Syntax is simple, no arguments:

```
# sbitx_controller
```


# Author

Rafael Diniz @ Rhizomatica

