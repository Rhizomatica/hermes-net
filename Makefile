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

.PHONY: clean install install_gateway install_v1 install_v2 trx_v1-firmware trx_v1-userland trx_v2-userland uuxcomp uucpd uucpd-v1 uucpd-v1 v1 v2

prefix=/usr

all: trx_v2-userland uuxcomp uucpd-v2

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

# compat
v2: all
install: install_v2

# build for v1
v1: uuxcomp uucpd-v1 trx_v1-userland

install_v2:
	$(MAKE) -C trx_v2-userland install
	$(MAKE) -C uuxcomp install
	IS_SBITX=1 $(MAKE) -C uucpd install
	install -m 644 -D system_services/init/uucpd.service $(DESTDIR)/etc/systemd/system/uucpd.service
	install -m 644 -D system_services/init/sbitx.service $(DESTDIR)/etc/systemd/system/sbitx.service
	install -m 644 -D system_services/alsa/asound-sbitx-hermes.conf $(DESTDIR)/etc/asound.conf
	install -m 644 -D system_services/udev/71-i2c.rules $(DESTDIR)/etc/udev/rules.d/71-i2c.rules
	install -D system_scripts/compression/compress_image.sh $(DESTDIR)$(prefix)/bin/compress_image.sh
	install -D system_scripts/compression/compress_audio.sh $(DESTDIR)$(prefix)/bin/compress_audio.sh
	install -D system_scripts/compression/decompress_image.sh $(DESTDIR)$(prefix)/bin/decompress_image.sh
	install -D system_scripts/compression/decompress_audio.sh $(DESTDIR)$(prefix)/bin/decompress_audio.sh
	install -D system_scripts/radio/sbitx_status $(DESTDIR)$(prefix)/bin/sbitx_status
	install -D system_scripts/email/mailkill.sh $(DESTDIR)$(prefix)/bin/mailkill.sh
	install -D system_scripts/email/mail_size_enforcement.sh $(DESTDIR)$(prefix)/bin/mail_size_enforcement.sh
	mkdir -p $(DESTDIR)/etc/sbitx
	install -D trx_v2-userland/config/core.ini $(DESTDIR)/etc/sbitx/core.ini
	install -D trx_v2-userland/config/user.ini $(DESTDIR)/etc/sbitx/user.ini
	mkdir -p $(DESTDIR)/etc/sbitx/web
	install -D trx_v2-userland/web/* $(DESTDIR)/etc/sbitx/web/
#	install -D system_scripts/uucpd/vara_watchdog.sh $(DESTDIR)$(prefix)/bin


install_v1:
	$(MAKE) -C trx_v1-userland install
	$(MAKE) -C uuxcomp install
	IS_SBITX=0 $(MAKE) -C uucpd install
	install -m 644 -D system_services/init/uucpd2.service $(DESTDIR)/etc/systemd/system/uucpd.service
	install -D system_scripts/compression/compress_image.sh $(DESTDIR)$(prefix)/bin/compress_image.sh
	install -D system_scripts/compression/compress_audio.sh $(DESTDIR)$(prefix)/bin/compress_audio.sh
	install -D system_scripts/compression/decompress_image.sh $(DESTDIR)$(prefix)/bin/decompress_image.sh
	install -D system_scripts/compression/decompress_audio.sh $(DESTDIR)$(prefix)/bin/decompress_audio.sh
	install -D system_scripts/email/mailkill.sh $(DESTDIR)$(prefix)/bin/mailkill.sh
	install -D system_scripts/email/mail_size_enforcement.sh $(DESTDIR)$(prefix)/bin/mail_size_enforcement.sh
#	install -D system_scripts/uucpd/vara_watchdog.sh $(DESTDIR)$(prefix)/bin


install_gateway:
	install -m 644 -D system_services/init/caller.service $(DESTDIR)/etc/systemd/system/caller.service
	install system_scripts/uucpd/caller.sh $(DESTDIR)$(prefix)/bin

install_mercury:
	install -m 644 -D system_services/init/uucpd-mercury.service $(DESTDIR)/etc/systemd/system/uucpd.service


clean:
	$(MAKE) -C trx_v2-userland clean
	$(MAKE) -C trx_v1-userland clean
#	$(MAKE) -C trx_v1-firmware clean
	$(MAKE) -C uuxcomp clean
	$(MAKE) -C uucpd clean
