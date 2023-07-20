#!/bin/bash

ubitx_client -c set_frequency -a 7150000
ubitx_client -c set_mode -a USB
ubitx_client -c set_bfo -a 11056650
ubitx_client -c set_mastercal -a 8850
ubitx_client -c set_ref_threshold -a 400
