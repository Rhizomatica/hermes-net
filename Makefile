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

.PHONY: clean install trx_v1-firmware trx_v1-userland uuxcomp uucpd

all: trx_v1-userland uuxcomp uucpd


trx_v1-firmware:
	$(MAKE) -C trx_v1-firmware

ispload: trx_v1-firmware
	$(MAKE) -C trx_v1-firmware ispload


trx_v1-userland:
	$(MAKE) -C trx_v1-userland

uuxcomp:
	$(MAKE) -C uuxcomp

uucpd:
	$(MAKE) -C uucpd


install: trx_v1-userland uuxcomp uucpd
	$(MAKE) -C trx_v1-userland install
	$(MAKE) -C uuxcomp install
	$(MAKE) -C uucpd install

install_gateway: install
	$(MAKE) -C uucpd install_gateway


clean:
	$(MAKE) -C trx_v1-userland clean
	$(MAKE) -C trx_v1-firmware clean
	$(MAKE) -C uuxcomp clean
	$(MAKE) -C uucpd clean
