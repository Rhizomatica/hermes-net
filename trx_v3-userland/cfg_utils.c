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

#include "cfg_utils.h"


// returns true if read successful
bool read_config_core(radio *radio_h, char *ini_name)
{
    dictionary  *   ini ;

    /* Some temporary variables to hold query results */
    int             b ;
    int             i ;
    double          d ;
    const char  *   s ;

    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return false;
    }
    iniparser_dump(ini, stderr);


    i = iniparser_getint(ini, ":bfo", 40035000);
    printf("BFO:      [%d]\n", i);
    radio_h->bfo_frequency = (uint32_t) i;

    i = iniparser_getint(ini, ":bridge_compensation", 100);
    printf("Bridge Compensation:      [%d]\n", i);
    radio_h->bridge_compensation = (uint32_t) i;

    i = iniparser_getint(ini, ":serial_number", -1);
    printf("Serial Number:      [%d]\n", i);
    radio_h->serial_number = (uint32_t) i;

    b = iniparser_getboolean(ini, ":enable_websocket", 0);
    printf("Enable Websocket:       [%d]\n", b);
    radio_h->enable_websocket = (bool) b;

    b = iniparser_getboolean(ini, ":enable_shm_control", 0);
    printf("Enable Shared Memory Control Interface:       [%d]\n", b);
    radio_h->enable_shm_control = (bool) b;

    s = iniparser_getstring(ini, ":i2c_dev", NULL);
    printf("I2C device:     [%s]\n", s ? s : "UNDEF");
    if (s)
        strcpy(radio_h->i2c_device, s);

    i = iniparser_getnsec(ini);
    printf("Number of Sections:     [%d]\n", i);

    for (int k = 0; k < i; k++)
    {
        char band_name[16];
        sprintf(band_name, "tx_band%d", k);
        if (iniparser_find_entry(ini, band_name))
            printf("Section exists:     [%s]\n", band_name);
    }

    iniparser_freedict(ini);

    return true;

}
//
bool write_config(radio *radio_h, char *ini_name)
{


    return true;

}
