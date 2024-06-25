# sBitx ATtiny85 firmware

To build, use `make`, to load use `make ispload`.

ATTinycore fetched upstream on 15/06/2024 from: https://github.com/SpenceKonde/ATTinyCore/

Based on original sBitx v3 ATiny85 firmware fetched in 15/06/2024 from: https://github.com/afarhan/sbitx/blob/main/swr_bridge.ino

This firmware will provide the following controls over I2C:
- Userland compatibility with stock firmware
- Read PB1 pin in both analog and digital modes
- Set PB1 to LOW or HIGH
- 24bit sBitx Serial number in the EEPROM (first 3 bytes)
- Access to reading and writing all 512 EEPROM bytes

TODO: host software

Tested with the SparkFun Tiny Programmer and default Debian 12 install. Install packages `arduino` and `arduino-mk`.

- Rafael Diniz
