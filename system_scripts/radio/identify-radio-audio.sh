#!/bin/bash
# Identify which ALSA sound card belongs to each radio by USB topology.
# The audio interface shares the same USB hub as the radio's serial adapter:
#   ICOM IC-7100: CP2102 (10c4:ea60) serial + Burr-Brown audio on same hub
#   Yaesu SCU-17: CP2105 (10c4:ea70) serial + C-Media audio on same hub

# Find the USB hub path for a given tty serial device
hub_for_tty() {
    local syspath
    syspath=$(udevadm info -q path -n "$1" 2>/dev/null) || return 1
    # Walk up: /devices/.../HUB/PORT/PORT:IF/ttyUSBn/tty/ttyUSBn → extract HUB
    echo "$syspath" | grep -oP '.*/\K[0-9]+-[0-9]+(\.[0-9]+)*(?=/[^/]+/[^/]+/ttyUSB)'
}

# Find which ALSA card lives on the same USB hub
card_on_hub() {
    local hub="$1"
    for card_path in /sys/class/sound/card*/device; do
        local dev_path
        dev_path=$(readlink -f "$card_path" 2>/dev/null) || continue
        if echo "$dev_path" | grep -q "/$hub/"; then
            basename "$(dirname "$card_path")" | sed 's/card//'
            return 0
        fi
    done
    return 1
}

# --- ICOM (CP2102, 10c4:ea60) ---
icom_tty=$(find /dev/serial/by-id/ -name '*CP2102*' -print -quit 2>/dev/null)
if [ -n "$icom_tty" ]; then
    icom_hub=$(hub_for_tty "$icom_tty")
    icom_card=$(card_on_hub "$icom_hub")
    if [ -n "$icom_card" ]; then
        echo "ICOM audio: hw:${icom_card},0 (hub $icom_hub)"
    else
        echo "ICOM serial found at $icom_hub but no audio on same hub"
    fi
else
    echo "ICOM: no CP2102 serial device found"
fi

# --- YAESU (CP2105 / SCU-17, 10c4:ea70) ---
yaesu_tty=$(find /dev/serial/by-id/ -name '*CP2105*' -print -quit 2>/dev/null)
if [ -n "$yaesu_tty" ]; then
    yaesu_hub=$(hub_for_tty "$yaesu_tty")
    yaesu_card=$(card_on_hub "$yaesu_hub")
    if [ -n "$yaesu_card" ]; then
        echo "YAESU audio: hw:${yaesu_card},0 (hub $yaesu_hub)"
    else
        echo "YAESU serial found at $yaesu_hub but no audio on same hub"
    fi
else
    echo "YAESU: no CP2105 serial device found"
fi
