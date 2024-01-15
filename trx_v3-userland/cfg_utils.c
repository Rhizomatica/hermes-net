/* hermes-net trxv3-userland
 *
 * Copyright (C) 2023 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
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

#include <iniparser.h>

#ifndef DEBUG_CFG_
#define DEBUG_CFG_ 1
#endif

#include "cfg_utils.h"

extern bool shutdown;

bool cfg_init(radio *radio_h, char *cfg_core, char *cfg_user, pthread_t *config_tid)
{
    init_config_core(radio_h, cfg_core);
    init_config_user(radio_h, cfg_user);

    radio_h->cfg_core_dirty = false;
    radio_h->cfg_user_dirty = false;

    // start config file writer thread
    pthread_create(config_tid, NULL, config_thread, (void *) radio_h);

    return true;
}

bool cfg_shutdown(radio *radio_h, pthread_t *config_tid)
{
    close_config_core(radio_h);
    close_config_user(radio_h);

    pthread_join(*config_tid, NULL);

    return true;
}

void *config_thread(void *radio_h_v)
{
    radio *radio_h = (radio *) radio_h_v;

    while (!shutdown)
    {
        if (radio_h->cfg_core_dirty)
        {
            write_config_core(radio_h, CFG_CORE_PATH);
            radio_h->cfg_core_dirty = false;
        }

        if (radio_h->cfg_user_dirty)
        {
            write_config_user(radio_h, CFG_USER_PATH);
            radio_h->cfg_user_dirty = false;
        }
        // every ~2s we check if the dirty bit is set
        sleep(2);
    }

    return NULL;

}

// returns true if read successful
bool init_config_core(radio *radio_h, char *ini_name)
{
    dictionary  *ini ;

    /* Some temporary variables to hold query results */
    int             b ;
    int             i ;
    double          d ;
    const char  *   s ;

    radio_h->cfg_core = NULL;
    ini = iniparser_load(ini_name);

    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return false;
    }
    iniparser_dump(ini, stderr);

    radio_h->cfg_core = ini;

    i = iniparser_getint(ini, "main:bfo", 40035000);
    printf("BFO:      [%d]\n", i);
    radio_h->bfo_frequency = (uint32_t) i;

    i = iniparser_getint(ini, "main:bridge_compensation", 100);
    printf("Bridge Compensation:      [%d]\n", i);
    radio_h->bridge_compensation = (uint32_t) i;

    i = iniparser_getint(ini, "main:serial_number", -1);
    printf("Serial Number:      [%d]\n", i);
    radio_h->serial_number = (uint32_t) i;

    b = iniparser_getboolean(ini, "main:enable_websocket", 0);
    printf("Enable Websocket:       [%d]\n", b);
    radio_h->enable_websocket = (bool) b;

    b = iniparser_getboolean(ini, "main:enable_shm_control", 0);
    printf("Enable Shared Memory Control Interface:       [%d]\n", b);
    radio_h->enable_shm_control = (bool) b;

    s = iniparser_getstring(ini, "main:i2c_dev", NULL);
    printf("I2C device:     [%s]\n", s ? s : "UNDEF");
    if (s)
        strcpy(radio_h->i2c_device, s);

    int sec_count = iniparser_getnsec(ini);
    sec_count--; // -1 to cope with the [main]
    printf("Number of Sections:     [%d]\n", sec_count);
    radio_h->band_power_count = (uint32_t) sec_count;

    for (int k = 0; k < sec_count; k++)
    {
        char band_name[32];
        sprintf(band_name, "tx_band%d", k);
        if (iniparser_find_entry(ini, band_name))
            printf("Section exists:     [%s]\n", band_name);

        sprintf(band_name, "tx_band%d:f_start", k);
        i = iniparser_getint(ini, band_name, -1);
        printf("%s:      [%d]\n", band_name, i);
        radio_h->band_power[k].f_start = i;

        sprintf(band_name, "tx_band%d:f_stop", k);
        i = iniparser_getint(ini, band_name, -1);
        printf("%s:      [%d]\n", band_name, i);
        radio_h->band_power[k].f_stop = i;

        sprintf(band_name, "tx_band%d:scale", k);
        d = iniparser_getdouble(ini, band_name, 0.0);
        printf("%s:      [%f]\n", band_name, d);
        radio_h->band_power[k].scale = d;
    }

    return true;
}

bool init_config_user(radio *radio_h, char *ini_name)
{
    dictionary  *   ini ;

    /* Some temporary variables to hold query results */
    int             b ;
    int             i ;
//    double          d ;
    const char  *   s ;

    radio_h->cfg_user = NULL;
    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return false;
    }
    iniparser_dump(ini, stderr);
    radio_h->cfg_user = ini;

    i = iniparser_getint(ini, "main:current_profile", 0);
    printf("current_profile:      [%d]\n", i);
    radio_h->profile_active_idx = (uint32_t) i;

    i = iniparser_getint(ini, "main:default_profile", 0);
    printf("default_profile:      [%d]\n", i);
    radio_h->profile_default_idx = (uint32_t) i;

    i = iniparser_getint(ini, "main:default_profile_fallback_timeout", -1);
    printf("default_profile_fallback_timeout:      [%d]\n", i);
    radio_h->profile_timeout = (int32_t) i;

    int sec_count = iniparser_getnsec(ini);
    sec_count--;
    printf("Number of Sections:     [%d]\n", sec_count);
    radio_h->profiles_count = sec_count;

    for (int k = 0; k < sec_count; k++)
    {
        char profile_name[32];
        char profile_field[64];

        sprintf(profile_name, "profile%d", k);
        if (iniparser_find_entry(ini, profile_name))
            printf("Section exists:     [%s]\n", profile_name);

        sprintf(profile_field, "%s:freq", profile_name);
        i = iniparser_getint(ini, profile_field, 7000000);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].freq = (uint32_t) i;

        sprintf(profile_field, "%s:mode", profile_name);
        s = iniparser_getstring(ini, profile_field, "USB");
        printf("%s:     [%s]\n", profile_field, s);
        if (!strcmp(s, "USB"))
            radio_h->profiles[k].mode = MODE_USB;
        else if (!strcmp(s, "LSB"))
            radio_h->profiles[k].mode = MODE_LSB;

        sprintf(profile_field, "%s:operating_mode", profile_name);
        i = iniparser_getint(ini, profile_field, 1);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].operating_mode = (uint16_t) i;

        sprintf(profile_field, "%s:mic_level", profile_name);
        i = iniparser_getint(ini, profile_field, 0);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].mic_level = (uint16_t) i;

        sprintf(profile_field, "%s:rx_level", profile_name);
        i = iniparser_getint(ini, profile_field, 100);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].rx_level = (uint16_t) i;

        sprintf(profile_field, "%s:speaker_level", profile_name);
        i = iniparser_getint(ini, profile_field, 0);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].speaker_level = (uint16_t) i;

        sprintf(profile_field, "%s:tx_level", profile_name);
        i = iniparser_getint(ini, profile_field, 0);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].tx_level = (uint16_t) i;

        sprintf(profile_field, "%s:agc", profile_name);
        s = iniparser_getstring(ini, profile_field, "OFF");
        printf("%s:     [%s]\n", profile_field, s);
        if (!strcmp(s, "OFF"))
            radio_h->profiles[k].agc = AGC_OFF;
        else if (!strcmp(s, "SLOW"))
            radio_h->profiles[k].agc = AGC_SLOW;
        else if (!strcmp(s, "MEDIUM"))
            radio_h->profiles[k].agc = AGC_MEDIUM;
        else if (!strcmp(s, "FAST"))
            radio_h->profiles[k].agc = AGC_FAST;

        sprintf(profile_field, "%s:compressor", profile_name);
        s = iniparser_getstring(ini, profile_field, "OFF");
        printf("%s:     [%s]\n", profile_field, s);
        if (!strcmp(s, "OFF"))
            radio_h->profiles[k].compressor = COMPRESSOR_OFF;
        else if (!strcmp(s, "ON"))
            radio_h->profiles[k].compressor = COMPRESSOR_OFF;

        sprintf(profile_field, "%s:bpf_low", profile_name);
        i = iniparser_getint(ini, profile_field, 50);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].bpf_low = (uint32_t) i;

        sprintf(profile_field, "%s:bpf_high", profile_name);
        i = iniparser_getint(ini, profile_field, 3000);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].bpf_high = (uint32_t) i;

        sprintf(profile_field, "%s:step_size", profile_name);
        i = iniparser_getint(ini, profile_field, 100);
        printf("%s:      [%d]\n", profile_field, i);
        radio_h->profiles[k].step_size = (uint32_t) i;

        sprintf(profile_field, "%s:enable_knob_volume", profile_name);
        b = iniparser_getboolean(ini, profile_field, 1);
        printf("%s:      [%d]\n", profile_field, b);
        radio_h->profiles[k].enable_knob_volume = (bool) b;

        sprintf(profile_field, "%s:enable_knob_frequency", profile_name);
        b = iniparser_getboolean(ini, profile_field, 1);
        printf("%s:      [%d]\n", profile_field, b);
        radio_h->profiles[k].enable_knob_frequency = (bool) b;

        sprintf(profile_field, "%s:enable_ptt", profile_name);
        b = iniparser_getboolean(ini, profile_field, 1);
        printf("%s:      [%d]\n", profile_field, b);
        radio_h->profiles[k].enable_ptt = (bool) b;

    }

    return true;
}

bool write_config_core(radio *radio_h, char *ini_name)
{
    FILE *f = fopen(ini_name, "w");
    iniparser_dump_ini(radio_h->cfg_core, f);
    fclose(f);

    return true;
}

bool write_config_user(radio *radio_h, char *ini_name)
{
    FILE *f = fopen(ini_name, "w");
    iniparser_dump_ini(radio_h->cfg_user, f);
    fclose(f);

    return true;
}

bool close_config_core(radio *radio_h)
{
    iniparser_freedict(radio_h->cfg_core);

    return true;
}

bool close_config_user(radio *radio_h)
{
    iniparser_freedict(radio_h->cfg_user);

    return true;
}
