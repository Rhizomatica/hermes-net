# UUCPD

UUCPD is a set of tools which allow UUCP to use ARDOP, VARA or Mercury as modem. With
this integration, UUCP is fully functional over HF links.

UUCPD comes with two tools: uucpd and uuport.

UUCPD is the daemon which keeps connected to ARDOP or VARA modem and properly
 receive calls (calling uucico) and initiate calls (uucico calls thought
 UUPORT connection).

UUPORT is the command invoked by UUCICO (using port type = pipe) when
initiating a call (uucico master mode). Communication between uuport and uucpd is done over shared memory.

## UUCPD Usage

Rhizomatica's uucpd version 0.3 by Rafael Diniz -  rafael (AT) rhizomatica (DOT) org
License: GNU AGPL version 3+
```
Usage modes: 
./uucpd -r vara -a tnc_ip_address -p tcp_base_port -s /dev/ttyUSB0 [-l]
./uucpd -h

Options:
 -r [ardop,vara]            Choose modem/radio type.
 -m                         Enable shared memory messaging parameter to uucp.
 -c callsign                Station Callsign (Eg: PU2HFF). Not setting it will cause the hostname to be retrieved from uucp config.
 -d remote_callsign         Remote Station Callsign.
 -a tnc_ip_address          IP address of the TNC,
 -p tnc_tcp_base_port       TNC's TCP base port of the TNC. ARDOP uses ports tcp_base_port and tcp_base_port+1.
 -t timeout                 Time to wait before disconnect when idling (only for ardop).
 -f features                   Supported features ARDOP: ofdm, noofdm (default: ofdm).
                               Supported features VARA, BW mode: 500, 2300 or 2750 (default: 2300).
                               Supported features VARA, P2P mode: "p" to enable (eg. 2300p).
 -s serial_device           Set the serial device file path for keying the radio (VARA ONLY).
 -l                         Tell UUCICO to ask login prompt (default: disabled).
 -F                         Enable UUCP handshake fast-track (auto-negotiated; falls back if peer not capable; no-login chat).
 -o [icom,ubitx,shm]        Sets radio type (supported: icom, ubitx or shm)
 -h                         Prints this help.
```

## UUPORT Usage

| Option | Description |
| --- | --- |
| -c system_name | Name of the remote system (default is don't change). |
| -e logfile.txt | Log file (default is stderr). |
| -h | Prints this help |

### Install

To compile and install, type:

    $ make
    $ make install

### Configuration

Port configuration example at "/etc/uucp/port":

    port HFP
    type pipe
    command /usr/bin/uuport

An alternative Port configuration if you use a patched uucp ( for "\Z"
support, available in "improved-pipe.patch" which was added to uucp debian 
package version 1.07-27 ), where uuport pass
the callsign of the station to be called to uucpd with the uucp remote
station name (allowing a single uucpd instance to be used for different
remote station callsigns):

    port HFP
    type pipe
    command /usr/bin/uuport -c \Z

Sys protocol example (tested and works fine) at "/etc/uucp/sys":

    protocol y
    protocol-parameter y packet-size 512
    protocol-parameter y timeout 540
    chat-timeout 200

Sys configuration example of remote system at "/etc/uucp/sys" (without login prompt):

    system remote
    call-login *
    call-password *
    time any
    port HFP
    chat "" \r

 Sys configuration example of remote system at "/etc/uucp/sys" (with login prompt - should call uucpd with "-l"):

    system remote
    call-login *
    call-password *
    time any
    port HFP
    chat "" \r\c ogin: \L word: \P

### Running uucpd

Examples of uucpd invocation:

    $ uucpd -a 127.0.0.1 -c PU2BBB -p 8515 -t 60 -r ardop
    $ uucpd -a 127.0.0.1 -p 8300 -r vara -o icom -s /dev/ttyUSB0 -f 2750

### Fast-track (-F)

Fast-track reduces over-the-air roundtrips by locally shortcutting the **pre-protocol UUCP DLE handshake**
(`Shere/S/ROK/P/U`). It is **OFF by default**.

Requirements / notes:
- Intended for **protocol `y`** (same as current recommended configuration).
- **No login prompt** mode only (do not use `uucpd -l`).
- Enable `-F` on **both ends** to get the benefit (it is auto-negotiated; if the peer is not capable it falls back
  to normal pass-through).

What you should see in logs:
- `uucp_fasttrack: probe start ...`
- then either `peer marker seen, activating` and `done (...)` (fast-track used),
- or `fallback to pass-through (...)` (normal UUCP handshake used).

### UUCP with "shared_messages.patch" for Raspberry OS (64 bits)

The "-m" option should only be used with patched uucp available in:
https://github.com/Rhizomatica/uucp

UUCPD support for the modified uucp (which has the "-m" patch to enable status messages) is optional. If you don't need this feature, just do not use "-m" option.
Packages of uucp with the shared memory for Raspberry OS's Debian 12 arm64 are available at:

    $ wget http://www.telemidia.puc-rio.br/~rafaeldiniz/public_files/hermes-repo/rafaeldiniz.gpg.key
    $ apt-key add rafaeldiniz.gpg.key
    $ echo deb http://www.telemidia.puc-rio.br/~rafaeldiniz/public_files/hermes-repo/ buster main >> /etc/apt/sources.list
    $ apt-get update
    $ apt-get install uucp

## C compiler defines

No specific C compiler define needs to be used to compile the code.

## Author

Rafael Diniz <rafael@riseup.net>

## License

GPLv3
