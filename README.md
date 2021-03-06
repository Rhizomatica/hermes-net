# hermes-net

Network subsystems of the HERMES telecommunication system: https://rhizomatica.org/hermes .


# Compile And Install

To compile the userland code, run:

* make

To install, run:

* make install

Additionally, if installing in a gateway station, run:

* make install_gateway

To compile the firmware, run:

* make trx_v1-firmware

To upload the firmware to the Arduino Nano based transceiver, run:

* make ispload


# Sub-projects

The repository contains all sub-projects which compose the hermes network stack.
Each entry in the list bellow contains a brief explanation of each directory
in this repository.

## uuxcomp

Uuxcomp is a uucp uux wrapper to be used with postfix email transport. It
compresses the email payload with xz and carries transcoding of specific formats, while
also providing an rmail wrapper to decompress the content at destination host.

## trx_v1-firmware

Rhizomatica's v1 µbitx-based radio transceiver Arduino Nano firmware.

## trx_v1-userland

Rhizomatica's v1 µbitx-based radio transceiver control utils. 

## uucpd

Previosly called rhizo-uuardopd, is composed by a control daemon which transports uucp payload
between uucico and the HF modem. It also contains uuport, a tool to be used in a uucico
port in pipe mode to connect uucico to the uucp daemon.

## system_scripts

This folder contains all the scripts needed by the HERMES system. Examples
include email management and media transcoding scripts.


## include

Contains header common to more than one project.

## system_scripts

Core scripts which performance many different tasks in network orchestration,
like email deletion and media transcoding.

## system_services

System service files, like init files and udev rules.

### Authors

Most of the code was written by Rafael Diniz <rafael@rhizomatica.org> with
contributions by Carlos Paulino <chgp@riseup.net>.

