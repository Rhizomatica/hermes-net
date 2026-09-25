#!/bin/bash
# configure-vara-icom.sh
# Configures VARA and UUCP to use Icom IC-7300 via USB on a Raspberry Pi 4

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

VARA_INI="/opt/VARA/VARA.ini"
VARA_INI_DEFAULT="/opt/VARA/VARA.ini.default"
UUCPD_DEFAULTS="/etc/default/uucpd"
UUCPD_DROPIN="/etc/systemd/system/uucpd.service.d/vara-icom.conf"
UDEV_RULES_SRC="${REPO_ROOT}/system_services/udev/99-radio.rules"
UDEV_RULES_DEST="/etc/udev/rules.d/99-radio.rules"

NEW_INPUT='Input Device Name=In: USB Audio CODEC - USB Audio'
NEW_OUTPUT='Output Device Name=Out: USB Audio CODEC - USB Audi'

NEW_UUCPD_OPTS='-a 127.0.0.1 -p 8300 -r vara -o icom7300 -s /dev/ICOM-CAT -f 2750p'

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

# ── Step 3: Point uucpd at the Icom and at VARA ─────────────────────────────
# The unit itself is the package's; the station's options go in
# /etc/default/uucpd and the dependency on VARA in a drop-in, so a reinstall
# of hermes-net keeps them.
echo "Configuring uucpd..."

if [[ -f "$UUCPD_DEFAULTS" ]]; then
    cp "$UUCPD_DEFAULTS" "${UUCPD_DEFAULTS}.bak"
    sed -i '/^UUCPD_OPTS=/d' "$UUCPD_DEFAULTS"
fi
echo "UUCPD_OPTS=\"${NEW_UUCPD_OPTS}\"" >> "$UUCPD_DEFAULTS"
echo "  Set UUCPD_OPTS in $UUCPD_DEFAULTS"

mkdir -p "$(dirname "$UUCPD_DROPIN")"
cat > "$UUCPD_DROPIN" <<'DROPIN'
# Written by configure-vara-icom.sh: uucpd talks to VARA, which runs under
# vnc.service, and keys the Icom itself over CAT.
[Unit]
Requires=vnc.service
After=vnc.service
DROPIN
echo "  Installed $UUCPD_DROPIN"

# ── Step 4: Install udev rules ───────────────────────────────────────────────
echo "Installing udev rules..."

if [[ ! -f "${UDEV_RULES_SRC}" ]]; then
    echo "  ERROR: udev rules source not found at ${UDEV_RULES_SRC}. Aborting."
    exit 1
fi

cp "${UDEV_RULES_SRC}" "${UDEV_RULES_DEST}"
echo "  Installed ${UDEV_RULES_DEST}"

udevadm control --reload-rules
udevadm trigger
echo "  udev rules reloaded."

# ── Step 5: Reload systemd and restart uucpd ─────────────────────────────────
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
echo "uucpd is set to use the Icom interface on /dev/ICOM-CAT."
