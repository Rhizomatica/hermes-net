#!/bin/sh
# switch_radio_controller.sh radiod|sbitx_controller [--no-modem]
#
# Switch the station between its two radio controllers, both already
# installed: hermes-radio-daemon (radiod.service, its make install) and the
# old sbitx_controller (sbitx.service, this repository's make
# install_sbitx_controller).  Only one runs: the other is stopped and
# disabled, so it stays off across reboots, and hermes-radio.service, the
# name the other units know the controller by, points at the new one.  The
# controller must open the snd-aloop cables before the modem, so the modem
# is stopped first and started after it.
#
#   --no-modem  leave the modem stopped, e.g. to measure with its test modes
#
# The web interface is built for one controller (hermes-installer's
# RADIO_DAEMON); this does not change it.

usage() { echo "usage: $0 radiod|sbitx_controller [--no-modem]" >&2; exit 1; }

case "$1" in
    radiod)                 unit=radiod.service; other=sbitx.service ;;
    sbitx_controller|sbitx) unit=sbitx.service;  other=radiod.service ;;
    *) usage ;;
esac
start_modem=true
case "$2" in
    "") ;;
    --no-modem) start_modem=false ;;
    *) usage ;;
esac

if ! systemctl cat "${unit}" >/dev/null 2>&1; then
    echo "$0: ${unit} is not installed" >&2
    exit 1
fi

systemctl stop modem.service 2>/dev/null
if systemctl cat "${other}" >/dev/null 2>&1; then
    systemctl disable --now "${other}"
    # sbitx_controller exits non-zero when stopped: not a failure here
    systemctl reset-failed "${other}" 2>/dev/null
fi
systemctl enable "${unit}"
/usr/lib/hermes-net/set_radio_controller.sh "${unit}" || exit 1

# The API and the scripts call the client sbitx_client.  radiod's installer
# links it to its own radio_client; installing sbitx_controller replaces the
# link with sbitx_controller's client.  Both speak the same shared memory.
if [ "${unit}" = radiod.service ] && [ -x /usr/bin/radio_client ]; then
    ln -sfn radio_client /usr/bin/sbitx_client
fi

systemctl restart hermes-radio.service
# The controller takes a few seconds to answer (a query right after the
# restart gets ERROR).
i=0
while [ ${i} -lt 20 ] && ! sbitx_client -c get_frequency -p 0 2>/dev/null | grep -q '^[0-9][0-9]*$'; do
    sleep 1; i=$((i + 1))
done
if ${start_modem}; then
    systemctl start modem.service
else
    systemctl stop modem.service
fi

for u in "${unit}" "${other}" modem.service; do
    printf '%s: %s\n' "${u}" "$(systemctl is-active "${u}" 2>/dev/null)"
done
