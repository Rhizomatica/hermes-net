#!/bin/sh
# Run by modem.service before the modem starts.
#
# The radio controller and the modem share snd-aloop cables, and a cable
# takes its period size from whichever end opens it first.  The controller
# must open them first: when the modem does, the controller is forced to the
# modem's period, its loopback stays in XRUN and the modem hears nothing.
# So wait, up to 15 s, until the controller has opened its playback end of
# the first Loopback card (hw:1,0 on a HERMES station), which carries the
# radio's audio to the modem.  Nothing to wait for when no controller runs.

systemctl -q is-active hermes-radio.service 2>/dev/null || exit 0

card=""
for c in /proc/asound/card*; do
    [ "$(cat "${c}/id" 2>/dev/null)" = "Loopback" ] && { card="${c}"; break; }
done
[ -n "${card}" ] || exit 0

i=0
while [ "${i}" -lt 150 ]; do
    case "$(head -n 1 "${card}/pcm0p/sub0/status" 2>/dev/null)" in
        closed|"") ;;
        *) exit 0 ;;
    esac
    sleep 0.1
    i=$((i + 1))
done
echo "wait_radio_audio: the radio controller has not opened ${card}, starting anyway" >&2
exit 0
