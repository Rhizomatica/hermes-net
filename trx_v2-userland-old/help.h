/* ubitx_client help
 * Copyright (C) 2022 Rhizomatica
 * Author: Rafael Diniz <rafael@rhizomatica.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

const char format_str[] = "* ptt_on\n"
"  * No Argument\n"
"  * Resp: OK | NOK | SWR | ERROR\n\n"

"* ptt_off\n"
"  * No Argument\n"
"  * Resp: OK | NOK | SWR | ERROR\n\n"

"* get_frequency\n"
"  * No Argument\n"
"  * Resp: Frequency | ERROR\n\n"

"* set_frequency\n"
"  * Frequency\n"
"  * Resp: OK | ERROR\n\n"

"* get_mode\n"
"  * No Argument\n"
"  * Resp: USB | LSB | CW | ERROR\n\n"

"* set_mode\n"
"  * LSB | USB | CW\n"
"  * Resp: OK | ERROR\n\n"

"* get_txrx_status\n"
"  * No Argument\n"
"  * Resp: INTX | INRX | ERROR\n\n"

"* get_protection_status\n"
"  * No Argument\n"
"  * Resp: PROTECTION_ON | PROTECTION_OFF | ERROR\n\n"

"* get_mastercal\n"
"  * No Argument\n"
"  * Resp: Frequency | ERROR\n\n"

"* set_mastercal\n"
"  * Frequency\n"
"  * Resp: OK | ERROR\n\n"

"* get_bfo\n"
"  * No Argument\n"
"  * Resp: Frequency | ERROR\n\n"

"* set_bfo\n"
"  * Frequency\n"
"  * Resp: OK | ERROR\n\n"

"* get_fwd\n"
"  * No Argument\n"
"  * Resp: Power (* 10 W) | ERROR\n\n"

"* get_ref\n"
"  * No Argument\n"
"  * Resp: SWR (* 10) | ERROR\n\n"

"* get_led_status\n"
"  * No Argument\n"
"  * Resp: LED_ON | LED_OFF | ERROR\n\n"

"* set_led_status\n"
"  * 0 | 1\n"
"  * Resp: OK | ERROR\n\n"

"* get_connected_status\n"
"  * No Argument\n"
"  * Resp: LED_ON | LED_OFF | ERROR\n\n"

"* set_connected_status\n"
"  * 0 | 1\n"
"  * Resp: OK | ERROR\n\n"

"* get_serial\n"
"  * No Argument\n"
"  * Resp: Serial Number | ERROR\n\n"

"* set_serial\n"
"  * Serial Number\n"
"  * Resp: OK | ERROR\n\n"

"* get_freqstep\n"
"  * No Argument\n"
"  * Resp: Frequency Step (Hz) | ERROR\n\n"

"* set_freqstep\n"
"  * Frequency Step (Hz)\n"
"  * Resp: OK | ERROR\n\n"

"* get_volume\n"
"  * No Argument\n"
"  * Resp: Volume (0-100) | ERROR\n\n"

"* set_volume\n"
"  * Volume (0-100)\n"
"  * Resp: OK | ERROR\n\n"

"* get_tone\n"
"  * No Argument\n"
"  * Resp: 0 (disabled) | 1 (enabled) | ERROR\n\n"

"* set_tone\n"
"  * Set transmit tone (0 to disable, 1 to enable)\n"
"  * Resp: OK | ERROR\n\n"


"* reset_protection\n"
"  * No Argument\n"
"  * Resp: OK | ERROR\n\n"

"* set_ref_threshold\n"
"  * Reflected Threshold Level For Protection Activation (0 - 1023)\n"
"  * Resp: OK | ERROR\n\n"

"* get_ref_threshold\n"
"  * No Argument\n"
"  * Resp: Reflected Threshold Level | ERROR\n\n"

"* gps_calibrate\n"
"  * No Argument\n"
"  * Resp: NO_GPS | ERROR\n\n"

"* set_radio_defaults\n"
"  * No Argument\n"
"  * Resp: OK (set default settings) | ERROR\n\n"

"* restore_radio_defaults\n"
"  * No Argument\n"
"  * Resp: OK (restore default settings) | ERROR\n\n"

"* get_status\n"
"  * No Argument\n"
"  * Resp: 3 lines output with a constant word and a response:\n"
"  * PPS_STATUS FAIL\n"
"  * OFFSET_CAL_STATUS FAIL\n"
"  * LATEST_OFFSET_CAL Offset\n\n"

"* radio_reset\n"
"  * No Argument\n"
"  * Resp: OK (and the sbitx_controller exits immediately)\n\n";
