# rhizo-uuardop
RHIZO-UUARDOP is a set of tools which allow UUCP to use ARDOP or VARA as modem. With
this integration, UUCP is fully functional over HF links.

Rhizo-uuardop comes with two tools: uuardopd and uuport.

UUARDOPD is the daemon which keeps connected to ARDOP or VARA modem and properly
 receive calls (calling uucico) and initiate calls (uucico calls thought
 UUPORT connection).

UUPORT is the command invoked by UUCICO (using port type = pipe) when
initiating a call (uucico master mode). Communication between uuport and uuardopd is done over shared memory.

## UUARDOPD Usage

| Option | Description |
| --- | --- |
| -c callsign | Station Callsign (Eg: PU2HFF) |
| -d remote_callsign | Remote Station Callsign (optional) |
| -r [ardop,vara] | Choose modem/radio type |
| -a tnc_ip_address | IP address of the TNC |
| -p tcp_base_port | TCP base port of the TNC. ARDOP uses ports tcp_base_port and tcp_base_port+1 |
| -t timeout | Time to wait before disconnect when idling (ARDOP ONLY) |
| -f features | Enable/Disable features. |
|  | Supported features ARDOP: ofdm, noofdm (default: ofdm) |
|  | Supported features VARA, BW mode: 500, 2300 or 2750 (default: 2300) |
| -s serial_device | Set the serial device file path for keying the radio (VARA ONLY) |
| -l | Tell UUCICO to ask login prompt (default: disabled) |
| -o [icom,ubitx,shm] | Sets radio type (supported: icom, ubitx or shm). |
| -h | Prints this help |


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
the callsign of the station to be called to uuardopd with the uucp remote
station name (allowing a single uuardopd instance to be used for different
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

 Sys configuration example of remote system at "/etc/uucp/sys" (with login prompt - should call uuardopd with "-l"):

    system remote
    call-login *
    call-password *
    time any
    port HFP
    chat "" \r\c ogin: \L word: \P

### Running uuardopd

Examples of uuardopd invocation:

    $ uuardopd -a 127.0.0.1 -c PU2BBB -p 8515 -t 60 -r ardop
    $ uuardopd -a 127.0.0.1 -p 8300 -r vara -o icom -s /dev/ttyUSB0 -f 2750

### UUCP with "improved-pipe.patch" for Raspberry OS (32 bits)

While UUCP package for Debian 11 (Bullseye) and onwards already have the patch included and the package works fine in Debian 10 (Buster), in the case of the Raspberry Pi Zero and 1, there is a need for a UUCP armhf package compiled for armv6l (Debian's armhf port is compiled for armv7l then incompatible with the Pi Zero and 1). We made available UUCP for Raspberry OS, and to get it installed do:

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
