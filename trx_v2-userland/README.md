# Userland For HERMES Radio Transceiver Version 2

This repository contains the HERMES userland utils for the sBitx v3 (or newer)
radio, which contains all the sBitx's control
code, and also the audio I/O management, which address the radio I/O
and loopback devices I/O. Code compiles and provides two executables: sbitx_controller, 
a daemon which talks to the radio I/O and does the audio DSP, and sbitx_client, which
provides radio control over the command line. A shared memory interface is also provided,
and a WebSocket streaming with radio parameters is also included (ps: edit sbitx_core.h and 
set the appropriate TLS certificate's and key's paths in CFG_SSL_CERT and CFG_SSL_KEY).

The following features can be highlighted:

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

Also load the i2c-dev module at boot (as root):

* echo i2c-dev >> /etc/modules

For detecting the I2C interfaces use:

```
i2cdetect -l

```

With gpio and RTC, it lists as (in a Pi 4):

```
i2c-22  i2c             i2c-gpio-rtc@0                          I2C adapter
```

By looking the I2C bus where the I2C BB driver is loaded, use the appropriate device file, eg., /dev/i2c-22 or /dev/i2c-2, in "/etc/sbitx/core.ini".


It is also strongly recommended to use the loopback locked to the wm8731. Add the following
parameter to snd-aloop module:

```
timer_source=hw:0,0
```

This code is designed to run on ARM64 (aarch64) Linux architecture.

# Compilation

Dependencies are, for example, in a Debian-based system:

```
apt-get install libiniparser-dev libfftw3-dev  libfftw3-double3 libssl-dev libi2c-dev
```

And csdr from https://github.com/Rhizomatica/csdr

Just type "make" to build both binaries sbitx_client and sbitx_controller.


# Usage


## sbitx_client commands

Syntax:
* sbitx_client -c command [-a command_argument] [-p profile_number]

Examples:
* sbitx_client -c set_frequency -a 7100000 -p 0
* sbitx_client -c get_frequency -p 0

Use the "-h" parameter for a full help.

## sbitx_controller commands

Sbitx_controller should be run as a daemon.
The "-h" parameter shows the help message and "-c" should be used to select the CPU to run.

Example:

* sbitx_controller


# RADEv2 Digital Voice Mode

This software supports RADEv2 (Radio Autoencoder Version 2) for digital voice
transmission over HF. When enabled, speech is encoded using the RADAE ML-based
codec and transmitted within the SSB passband.

## RADEv2 Prerequisites

1. Clone the RADAE repository:
```bash
cd /path/to/hermes-net
git clone https://github.com/drowe67/radae.git
git checkout dr-radev2
```

2. Build RADAE (read ../radae/README.md - THIS IS INCOMPLETE):
```bash
cd radae
mkdir build && cd build
cmake ..
make
```

3. Install Python dependencies:
```bash
pip3 install torch numpy matplotlib
```

4. Ensure the RADEv2 model files are present:
   - `radae/250725/checkpoints/checkpoint_epoch_200.pth` (main model)
   - `radae/250725a_ml_sync` (frame sync model)

## Building with RADEv2 Support

Build as usual - RADEv2 support is included automatically:
```bash
cd trx_v2-userland
make
```

## Using Digital Voice Mode

Enable digital voice via the sbitx_client:
```bash
sbitx_client -c set_digital_voice -a 1 -p 0    # Enable digital voice on profile 0
sbitx_client -c set_digital_voice -a 0 -p 0    # Disable digital voice
sbitx_client -c get_digital_voice -p 0         # Check digital voice status
```

When digital voice is enabled:
- **TX**: Speech from mic/loopback is encoded via LPCNet feature extraction,
  processed through the RADAE encoder, and the resulting modem waveform
  is placed in the SSB passband.
- **RX**: The received SSB signal is decoded by RADAE, and
  synthesized back to speech via LPCNet FARGAN synthesis.

## Technical Details

- Speech sample rate: 16kHz (LPCNet)
- Modem sample rate: 8kHz (RADAE)
- Radio sample rate: 96kHz
- RADEv2 parameters: latent_dim=56, w1_dec=128
- Model: 250725 checkpoint

The digital voice processing runs as subprocess pipelines:
- TX: `lpcnet_demo -features` → `inference.py` → modem
- RX: `radae_rxe.py` → `lpcnet_demo -fargan-synthesis` → speech

# Author

Rafael Diniz @ Rhizomatica

