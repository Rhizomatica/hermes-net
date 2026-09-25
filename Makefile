# hermes-net network subsystems for HERMES
# Copyright (C) 2019-2022 Rhizomatica
# Author: Rafael Diniz <rafael@riseup.net>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

.PHONY: clean install install_gateway install_v1 install_v2 install_mercury install_common install_sbitx_controller install_sbitx_hw install_loopback_audio common trx_v1-firmware trx_v1-userland trx_v2-userland uuxcomp uucpd uucpd-v1 uucpd-v2 v1 v2

prefix=/usr
export prefix
# Units are the package's; stations change them with drop-ins in
# /etc/systemd/system/<unit>.d/ and with /etc/default/uucpd, so that a
# reinstall never overwrites a station's settings.
unitdir=$(prefix)/lib/systemd/system

# What every station needs, whichever radio controller it runs.  Building
# uucpd for the sBitx only compiles trx_v2-userland/sbitx_io.o, the shared
# memory client, which hermes-radio-daemon speaks as well.  The old sBitx
# controller is opt-in: make trx_v2-userland install_sbitx_controller, or
# make v2 install_v2 for the whole sbitx_controller station.
all: common

common: uuxcomp uucpd-v2

trx_v1-firmware:
	$(MAKE) -C trx_v1-firmware

ispload: trx_v1-firmware
	$(MAKE) -C trx_v1-firmware ispload

trx_v2-userland:
	$(MAKE) -C trx_v2-userland

trx_v1-userland:
	$(MAKE) -C trx_v1-userland

uuxcomp:
	$(MAKE) -C uuxcomp

uucpd-v1:
	IS_SBITX=0 $(MAKE) -C uucpd

uucpd-v2:
	IS_SBITX=1 $(MAKE) -C uucpd

# Installs what every station needs; see install_v2 for the sBitx with
# sbitx_controller.
install: install_common install_loopback_audio

# the sBitx with sbitx_controller
v2: trx_v2-userland common

# build for v1
v1: uuxcomp uucpd-v1 trx_v1-userland

# The sBitx with sbitx_controller, as before.  A station running
# hermes-radio-daemon instead installs install_common and
# install_loopback_audio, plus install_sbitx_hw on an sBitx.
install_v2: install_common install_sbitx_controller install_sbitx_hw install_loopback_audio

# uucpd, uuxcomp, the helper scripts and the uucpd unit.  Owns no file of
# either radio controller.
install_common:
	$(MAKE) -C uuxcomp install
	IS_SBITX=1 $(MAKE) -C uucpd install
	install -m 644 -D system_services/init/uucpd.service $(DESTDIR)$(unitdir)/uucpd.service
	test -f $(DESTDIR)/etc/default/uucpd || install -m 644 -D system_services/default/uucpd $(DESTDIR)/etc/default/uucpd
	install -D system_scripts/compression/compress_image.sh $(DESTDIR)$(prefix)/bin/compress_image.sh
	install -D system_scripts/compression/compress_audio.sh $(DESTDIR)$(prefix)/bin/compress_audio.sh
	install -D system_scripts/compression/decompress_image.sh $(DESTDIR)$(prefix)/bin/decompress_image.sh
	install -D system_scripts/compression/decompress_audio.sh $(DESTDIR)$(prefix)/bin/decompress_audio.sh
	install -D system_scripts/radio/sbitx_status $(DESTDIR)$(prefix)/bin/sbitx_status
	install -D system_scripts/email/mailkill.sh $(DESTDIR)$(prefix)/bin/mailkill.sh
	install -D system_scripts/email/mail_size_enforcement.sh $(DESTDIR)$(prefix)/bin/mail_size_enforcement.sh
#	install -D system_scripts/uucpd/vara_watchdog.sh $(DESTDIR)$(prefix)/bin

# The old controller: sbitx_controller, sbitx_client, its unit and
# /etc/sbitx.  Its configuration is only installed where there is none, so
# a reinstall keeps the station's.  Installing it replaces
# hermes-radio-daemon as the station's controller (take_over_radio.sh),
# as installing hermes-radio-daemon replaces it.
install_sbitx_controller:
	$(MAKE) -C trx_v2-userland install
	install -m 644 -D system_services/init/sbitx.service $(DESTDIR)$(unitdir)/sbitx.service
	mkdir -p $(DESTDIR)/etc/sbitx
	test -f $(DESTDIR)/etc/sbitx/core.ini || install -D trx_v2-userland/config/core.ini $(DESTDIR)/etc/sbitx/core.ini
	test -f $(DESTDIR)/etc/sbitx/user.ini || install -D trx_v2-userland/config/user.ini $(DESTDIR)/etc/sbitx/user.ini
	mkdir -p $(DESTDIR)/etc/sbitx/web
	install -D trx_v2-userland/web/* $(DESTDIR)/etc/sbitx/web/
	install -D system_scripts/radio/take_over_radio.sh $(DESTDIR)$(prefix)/lib/hermes-net/take_over_radio.sh
	if [ -z "$(DESTDIR)" ]; then $(prefix)/lib/hermes-net/take_over_radio.sh; fi

# The sBitx hardware, needed with either controller: the name of the
# radio's i2c bus.
install_sbitx_hw:
	install -m 644 -D system_services/udev/71-i2c.rules $(DESTDIR)/etc/udev/rules.d/71-i2c.rules

# The modem's audio: the radio controller (sbitx_controller, or
# hermes-radio-daemon with either backend) bridges the radio's codec to the
# snd-aloop cards, and this makes them the modem's default device.
install_loopback_audio:
	install -m 644 -D system_services/alsa/asound-sbitx-hermes.conf $(DESTDIR)/etc/asound.conf

install_v1:
	$(MAKE) -C trx_v1-userland install
	$(MAKE) -C uuxcomp install
	IS_SBITX=0 $(MAKE) -C uucpd install
	install -m 644 -D system_services/init/uucpd.service $(DESTDIR)$(unitdir)/uucpd.service
	test -f $(DESTDIR)/etc/default/uucpd || install -m 644 -D system_services/default/uucpd $(DESTDIR)/etc/default/uucpd
	install -D system_scripts/compression/compress_image.sh $(DESTDIR)$(prefix)/bin/compress_image.sh
	install -D system_scripts/compression/compress_audio.sh $(DESTDIR)$(prefix)/bin/compress_audio.sh
	install -D system_scripts/compression/decompress_image.sh $(DESTDIR)$(prefix)/bin/decompress_image.sh
	install -D system_scripts/compression/decompress_audio.sh $(DESTDIR)$(prefix)/bin/decompress_audio.sh
	install -D system_scripts/email/mailkill.sh $(DESTDIR)$(prefix)/bin/mailkill.sh
	install -D system_scripts/email/mail_size_enforcement.sh $(DESTDIR)$(prefix)/bin/mail_size_enforcement.sh
#	install -D system_scripts/uucpd/vara_watchdog.sh $(DESTDIR)$(prefix)/bin


install_gateway:
	install -m 644 -D system_services/init/caller.service $(DESTDIR)$(unitdir)/caller.service
	install system_scripts/uucpd/caller.sh $(DESTDIR)$(prefix)/bin

install_mercury:
	install -m 644 -D system_services/init/modem.service $(DESTDIR)$(unitdir)/modem.service


clean:
	$(MAKE) -C trx_v2-userland clean
	$(MAKE) -C trx_v1-userland clean
#	$(MAKE) -C trx_v1-firmware clean
	$(MAKE) -C uuxcomp clean
	$(MAKE) -C uucpd clean
