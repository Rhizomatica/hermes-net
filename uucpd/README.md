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
 -F                         Use the pre-agreed UUCP startup (passes -Y to uucico; requires both ends).
 -o [icom,icom7300,ubitx,shm,none] Sets radio type (`icom` uses IC-7100 defaults, `icom7300` uses IC-7300 defaults)
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
    $ uucpd -a 127.0.0.1 -p 8300 -r vara -o icom7300 -s /dev/ttyUSB0 -f 2750

`-o icom` keeps the existing IC-7100 CI-V defaults (19200 baud, address `0x88`).
Use `-o icom7300` for IC-7300 defaults (115200 baud, address `0x94`).

### Pre-agreed startup (-F)

`-F` makes uucpd pass `-Y` to uucico, which replaces the pre-protocol UUCP DLE handshake
(`Shere` / `S` / `ROK` / `P` / `U`) with a single `Y` command. On a half duplex HF link that is
the difference between six turnarounds and two before any payload moves. It is **OFF by default**.

`DLE` is the ASCII **Data Link Escape** character (`0x10`, octal `\020`, Ctrl-P) used by UUCP to frame
these pre-protocol commands (each is sent as `<DLE>ASCII...<NUL>`, e.g. `\020Shere=hostname\000`).

On the wire, a normal session versus a pre-agreed one:

| | normal | `-F` |
| --- | --- | --- |
| TX1 | caller: chat script (`\r`) | caller: `\020Yy CALLSIGN -N0nnn\0` |
| TX2 | called: `\020Shere=NAME\0` | called: `\020Yy -N0nnn\0` + protocol sync |
| TX3 | caller: `\020SNAME -R -N0nnn\0` | caller: protocol sync |
| TX4 | called: `\020ROKN0nnn\0` + `\020Py\0` | *(payload)* |
| TX5 | caller: `\020Uy\0` | |
| TX6 | both: protocol `y` sync exchange | |

The caller puts its own system name in the `Y` command, so the called station still looks the system
up in its `sys` file exactly as it would from an `S` command — per-system permissions, spool
directories and aliases all keep working, and uucpd does not have to tell uucico who called.

Requirements / notes:
- The protocol and packet size still come from `/etc/uucp/sys` (`protocol y`,
  `protocol-parameter y packet-size 512`); `-F` changes nothing there.
- **No login prompt** mode only (do not combine with `uucpd -l`).
- Requires a uucico with `-Y` support (Rhizomatica uucp 1.07-36 or later; see below).
- This mode performs **no UUCP-layer authentication**, exactly like the recommended
  `chat "" \r` no-login configuration it replaces.
- It is for the HF links only. There is nothing to gain on the TCP uplink to the email
  server, where the saving — line turnarounds — is not something a full duplex link pays for.

**Interoperability.** A called station running `-F` detects a caller that is not, and falls back to
the normal handshake with no delay, so you do **not** need a flag day:

| caller \ called | normal | `-F` |
| --- | --- | --- |
| normal | works | works (falls back) |
| `-F` | **fails** | fast path |

Roll out **gateways first** (add `-F` to the uucpd command line), then enable `-Y` on client
stations one at a time by setting `UUCICO_HF_OPTS="-Y"` at the top of `caller.sh`, which applies
it to the HF calls only.

What you should see in the uucico logs:
- caller: `Login successful (pre-agreed)`
- called: `Pre-agreed startup from caller (protocol 'y')`
- called, when talking to a station that is not using it:
  `No pre-agreed startup from caller (caller sent a chat script), using normal handshake`

### UUCP with "shared_messages.patch" for Raspberry OS (64 bits)

The "-m" option should only be used with patched uucp available in:
https://github.com/Rhizomatica/uucp

UUCPD support for the modified uucp (which has the "-m" patch to enable status messages) is optional. If you don't need this feature, just do not use "-m" option.
Packages of uucp with the shared memory for Debian 13 arm64 (RaspiOS) and amd64 are available at: https://debian.hermes.radio

## C compiler defines

No specific C compiler define needs to be used to compile the code.

## Author

Rafael Diniz <rafael@riseup.net>

## License

GPLv3
