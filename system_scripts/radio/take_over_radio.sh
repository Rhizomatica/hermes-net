#!/bin/sh
# Make sbitx_controller the station's radio controller in place of
# hermes-radio-daemon (radiod): installing one controller replaces the
# other.  Both provide the same shared memory, so the modem, uucpd, the
# API and sbitx_client keep working unchanged.  Run on install of
# sbitx_controller; does nothing where systemd is not running (a chroot,
# a package build).

[ -d /run/systemd/system ] || exit 0

systemctl daemon-reload || true
if systemctl list-unit-files radiod.service >/dev/null 2>&1; then
    systemctl disable --now radiod.service >/dev/null 2>&1 || true
fi
systemctl enable sbitx.service >/dev/null 2>&1 || true
systemctl start sbitx.service || true
