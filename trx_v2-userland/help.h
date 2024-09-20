/* sbitx_client help
 * Copyright (C) 2022-2024 Rhizomatica
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

const char format_str[] =
"* ptt_on\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK | NOK | SWR | ERROR\n\n"

"* ptt_off\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK | NOK | SWR | ERROR\n\n"

"* get_frequency\n"
"  * Specify profile\n"
"  * No Argument\n"
"  * Resp: Frequency | ERROR\n\n"

"* set_frequency\n"
"  * Specify profile\n"
"  * Frequency\n"
"  * Resp: OK | ERROR\n\n"

"* get_mode\n"
"  * Specify profile\n"
"  * No Argument\n"
"  * Resp: USB | LSB | CW | ERROR\n\n"

"* set_mode\n"
"  * Specify profile\n"
"  * LSB | USB | CW\n"
"  * Resp: OK | ERROR\n\n"

"* get_txrx_status\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: INTX | INRX | ERROR\n\n"

"* get_protection_status\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: PROTECTION_ON | PROTECTION_OFF | ERROR\n\n"

"* get_fwd\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Power (* 10 W) | ERROR\n\n"

"* get_ref\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: SWR (* 10) | ERROR\n\n"

"* get_led_status\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: LED_ON | LED_OFF | ERROR\n\n"

"* set_led_status\n"
"  * Do not specify profile\n"
"  * 0 | 1\n"
"  * Resp: OK | ERROR\n\n"

"* get_connected_status\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: LED_ON | LED_OFF | ERROR\n\n"

"* set_connected_status\n"
"  * Do not specify profile\n"
"  * 0 | 1\n"
"  * Resp: OK | ERROR\n\n"

"* get_serial\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Serial Number | ERROR\n\n"

"* set_serial\n"
"  * Do not specify profile\n"
"  * Serial Number\n"
"  * Resp: OK | ERROR\n\n"

"* get_profile\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Profile Number | ERROR\n\n"

"* set_profile\n"
"  * Do not specify profile\n"
"  * Profile Number\n"
"  * Resp: OK | ERROR\n\n"

"* get_freqstep\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Frequency Step (Hz) | ERROR\n\n"

"* set_freqstep\n"
"  * Do not specify profile\n"
"  * Frequency Step (Hz)\n"
"  * Resp: OK | ERROR\n\n"

"* get_volume\n"
"  * Specify profile\n"
"  * No Argument\n"
"  * Resp: Volume (0-100) | ERROR\n\n"

"* set_volume\n"
"  * Specify profile\n"
"  * Volume (0-100)\n"
"  * Resp: OK | ERROR\n\n"

"* get_tone\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: 0 (disabled) | 1 (enabled) | ERROR\n\n"

"* set_tone\n"
"  * Do not specify profile\n"
"  * Set transmit tone (0 to disable, 1 to enable)\n"
"  * Resp: OK | ERROR\n\n"

"* reset_protection\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK | ERROR\n\n"

"* reset_timeout\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK | ERROR\n\n"

"* set_timeout\n"
"  * Do not specify profile\n"
"  * Set timeout (in seconds) for changing to default profile (-1 disables timeout)\n"
"  * Resp: OK | ERROR\n\n"

"* get_timeout\n"
"  * Do not specify profile\n"
"  * Get timeout (in seconds) for changing to default profile (-1 means disabled timeout)\n"
"  * Resp: OK | ERROR\n\n"

"* set_ref_threshold\n"
"  * Do not specify profile\n"
"  * Reflected Threshold Level (SWR x 10) For Protection Activation (0 - 1023)\n"
"  * Resp: OK | ERROR\n\n"

"* get_ref_threshold\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Reflected Threshold Level | ERROR\n\n"

"* get_bitrate\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Modem Bitrate | ERROR\n\n"

"* set_bitrate\n"
"  * Do not specify profile\n"
"  * Modem Bitrate\n"
"  * Resp: OK | ERROR\n\n"

"* get_snr\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: Modem SNR | ERROR\n\n"

"* set_snr\n"
"  * Do not specify profile\n"
"  * Modem SNR\n"
"  * Resp: OK | ERROR\n\n"

"* set_radio_defaults\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK (set default settings) | ERROR\n\n"
#if 0
"* restore_radio_defaults\n"
"  * No Argument\n"
"  * Resp: OK (restore default settings) | ERROR\n\n"
#endif
"* radio_reset\n"
"  * Do not specify profile\n"
"  * No Argument\n"
"  * Resp: OK (and the sbitx_controller exits immediately)\n\n";
