#!/bin/bash
# configure-vara-icom.sh
# Configures VARA and UUCP to use Icom IC-7300 via USB on a Raspberry Pi 4

set -euo pipefail

VARA_INI="/opt/VARA/VARA.ini"
VARA_INI_DEFAULT="/opt/VARA/VARA.ini.default"
UUCPD_SERVICE="/etc/systemd/system/uucpd.service"

NEW_INPUT='Input Device Name=In: USB Audio CODEC - USB Audio'
NEW_OUTPUT='Output Device Name=Out: USB Audio CODEC - USB Audi'

NEW_EXECSTART='ExecStart=/usr/bin/uucpd -a 127.0.0.1 -p 8300 -r vara -o icom -s /dev/ttyUSB0 -f 2750p'

# ── Step 1: Update VARA audio device names ────────────────────────────────────
echo "Updating VARA audio device names..."

for FILE in "$VARA_INI" "$VARA_INI_DEFAULT"; do
    if [[ ! -f "$FILE" ]]; then
        echo "  WARNING: $FILE not found, skipping."
        continue
    fi

    cp "$FILE" "${FILE}.bak"

    sed -i "s|^Input Device Name=In:.*|${NEW_INPUT}|" "$FILE"
    sed -i "s|^Output Device Name=Out:.*|${NEW_OUTPUT}|" "$FILE"

    echo "  Updated: $FILE  (backup: ${FILE}.bak)"
done

# ── Step 2: Stop and disable sbitx.service ────────────────────────────────────
echo "Stopping and disabling sbitx.service..."

if systemctl is-active --quiet sbitx.service; then
    systemctl stop sbitx.service
    echo "  sbitx.service stopped."
else
    echo "  sbitx.service was not running."
fi

if systemctl is-enabled --quiet sbitx.service 2>/dev/null; then
    systemctl disable sbitx.service
    echo "  sbitx.service disabled."
else
    echo "  sbitx.service was already disabled."
fi

# ── Step 3: Update uucpd.service ExecStart line ───────────────────────────────
echo "Updating uucpd.service..."

if [[ ! -f "$UUCPD_SERVICE" ]]; then
    echo "  ERROR: $UUCPD_SERVICE not found. Aborting."
    exit 1
fi

cp "$UUCPD_SERVICE" "${UUCPD_SERVICE}.bak"

sed -i "s|^ExecStart=.*|${NEW_EXECSTART}|" "$UUCPD_SERVICE"
echo "  Updated ExecStart in $UUCPD_SERVICE  (backup: ${UUCPD_SERVICE}.bak)"

sed -i 's|^After=.*|After=vnc.service|' "$UUCPD_SERVICE"
sed -i 's|^Requires=.*|Requires=vnc.service|' "$UUCPD_SERVICE"
echo "  Updated After= and Requires= to vnc.service"

# ── Step 4: Reload systemd and restart uucpd ─────────────────────────────────
echo "Reloading systemd daemon..."
systemctl daemon-reload

echo "Restarting uucpd vnc and x11 services..."
systemctl stop uucpd.service
systemctl stop vnc.service
systemctl stop x11.service

systemctl start x11.service
echo "  x11.service restarted."
systemctl start vnc.service
echo "  vnc.service restarted."
systemctl start uucpd.service
echo "  uucpd.service restarted."

echo ""
echo "Done. VARA is now configured to use the USB Audio CODEC (Icom IC-7300)."
echo "uucpd is set to use the Icom interface on /dev/ttyUSB0."
