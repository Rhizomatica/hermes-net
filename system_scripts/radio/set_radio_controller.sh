#!/bin/sh
# set_radio_controller.sh <sbitx.service|radiod.service>
#
# Make <unit> the station's radio controller as far as the other HERMES
# units are concerned: they refer to the controller only as
# hermes-radio.service, an alias (a symlink in /etc/systemd/system) that this
# points at the unit of the controller the station runs.

unit="$1"
case "${unit}" in
    sbitx.service|radiod.service) ;;
    *) echo "usage: $0 sbitx.service|radiod.service" >&2; exit 1 ;;
esac

for dir in /etc/systemd/system /usr/lib/systemd/system /lib/systemd/system; do
    if [ -f "${dir}/${unit}" ] && [ ! -L "${dir}/${unit}" ]; then
        ln -sfn "${dir}/${unit}" /etc/systemd/system/hermes-radio.service
        [ -d /run/systemd/system ] && systemctl daemon-reload
        exit 0
    fi
done
echo "$0: no unit file for ${unit}" >&2
exit 1
