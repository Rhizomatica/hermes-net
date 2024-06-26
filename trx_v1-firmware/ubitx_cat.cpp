/* Rhizomatica Firmware for uBitx v6 based Rhizomatica HF transceiver
 *
 * Copyright (C) 2021-2022 Rhizomatica
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


#include <Arduino.h>
#include "ubitx.h"
#include "ubitx_cat.h"
#include "../include/radio_cmds.h"

#if RADUINO_VER > 0
#include "ubitx_calibration.h"
#endif

/* reset function declaration */
void(*resetFunc) (void) = 0;


/**
 * The CAT protocol is ad-hoc, made for Rhizomatica's HF transceiver,
 * which provides remote control by computers through the serial port.
 *
 */


static uint32_t rxBufferArriveTime = 0;
static uint8_t rxBufferCheckCount = 0;

static uint8_t cat[5];

uint16_t skipTimeCount = 0;


void processCATCommand(byte* cmd)
{
    uint8_t response[5];

    switch(cmd[4]){

    case CMD_PTT_ON: // PTT On
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else if (!inTx)
        {
            response[0] = CMD_RESP_ACK;
            startTx();
        } else
        {
            response[0] = CMD_RESP_PTT_ON_NACK;
        }
        Serial.write(response,1);
        break;

    case CMD_PTT_OFF: // PTT OFF
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else if (inTx)
        {
            response[0] = CMD_RESP_ACK;
            stopTx();
        }
        else
        {
            response[0] = CMD_RESP_PTT_OFF_NACK;
        }
        Serial.write(response,1);
        break;

    case CMD_RESET_PROTECTION: // RESET PROTECTION
        response[0] = CMD_RESP_ACK;
        triggerProtectionReset();
        Serial.write(response,1);
        break;

    case CMD_GET_FREQ: // GET FREQUENCY
        response[0] = CMD_RESP_GET_FREQ_ACK;
        memcpy(response+1, &frequency, 4);
        Serial.write(response,5);
        break;

    case CMD_SET_FREQ: // SET FREQUENCY
        memcpy(&frequency, cmd, 4);
        setFrequency(frequency);
        saveVFOs();
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_SET_MODE: // set mode
        if (cmd[0] == 0x00 || cmd[0] == 0x03)
            isUSB = 0;
        else
            isUSB = 1;

        saveVFOs();
        setFrequency(frequency);

        response[0] = CMD_RESP_ACK;
        Serial.write(response, 1);
        break;

    case CMD_GET_MODE: // GET SSB MODE
        if (isUSB)
            response[0] = CMD_RESP_GET_MODE_USB;
        else
            response[0] = CMD_RESP_GET_MODE_LSB;
        Serial.write(response,1);
        break;

    case CMD_GET_TXRX_STATUS: // GET TX/RX STATUS
        if (inTx)
            response[0] = CMD_RESP_GET_TXRX_INTX;
        else
            response[0] = CMD_RESP_GET_TXRX_INRX;
        Serial.write(response,1);
        break;

    case CMD_GET_PROTECTION_STATUS: // GET PROTECTION STATUS
        if (is_swr_protect_enabled)
            response[0] = CMD_RESP_GET_PROTECTION_ON;
        else
            response[0] = CMD_RESP_GET_PROTECTION_OFF;
        Serial.write(response,1);
        break;

    case CMD_GET_MASTERCAL: // GET MASTER CAL
        response[0] = CMD_RESP_GET_MASTERCAL_ACK;
        memcpy(response+1, &calibration, 4);
        Serial.write(response,5);
        break;

    case CMD_SET_MASTERCAL: // SET MASTER CAL
        memcpy(&calibration, cmd, 4);
        setMasterCal(calibration);
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_GET_BFO: // GET BFO
        response[0] = CMD_RESP_GET_BFO_ACK;
        memcpy(response+1, &usbCarrier, 4);
        Serial.write(response,5);
        break;

    case CMD_SET_BFO: // SET BFO
        memcpy(&usbCarrier, cmd, 4);
        setBFO(usbCarrier);
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_GET_FWD: // GET FWD
        response[0] = CMD_RESP_GET_FWD_ACK;
        memcpy(response+1, &forward, 2);
        Serial.write(response,5);
        break;

    case CMD_GET_REF: // GET REF
        response[0] = CMD_RESP_GET_REF_ACK;
        memcpy(response+1, &reflected, 2);
        Serial.write(response,5);
        break;

    case CMD_GET_LED_STATUS: // GET LED STATUS
        if (led_status)
            response[0] = CMD_RESP_GET_LED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_LED_STATUS_OFF;
        Serial.write(response,1);
        break;

    case CMD_SET_LED_STATUS: // SET LED STATUS
        setLed(cmd[0]);
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_GET_CONNECTED_STATUS: // GET CONNECTED STATUS
        if (connected_status)
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_OFF;
        Serial.write(response,1);
      break;

    case CMD_SET_CONNECTED_STATUS: // SET CONNECTED STATUS
        setConnected(cmd[0]);
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_GET_SERIAL: // GET SERIAL NUMBER
        response[0] = CMD_RESP_GET_SERIAL_ACK;
        memcpy(response+1, &serial, 4);
        Serial.write(response,5);
      break;

    case CMD_SET_SERIAL: // SET SERIAL NUMBER
        memcpy(&serial, cmd, 4);
        setSerial(serial);
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_SET_REF_THRESHOLD: // SET REF THRESHOLD
        memcpy(&reflected_threshold, cmd, 2);
        save_reflected_threshold();
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_GET_REF_THRESHOLD: // GET REF THRESHOLD
        response[0] = CMD_RESP_GET_REF_THRESHOLD_ACK;
        memcpy(response+1, &reflected_threshold, 2);
        Serial.write(response,5);
        break;

    case CMD_GPS_CALIBRATE: // CMD_GPS_CALIBRATE
#if RADUINO_VER > 0
        if (calibration_enabled == false)
            enable_calibration();
        response[0] = CMD_RESP_ACK;
#else
        response[0] = CMD_RESP_GPS_NOT_PRESENT;
#endif
        Serial.write(response,1);
        break;

    case CMD_GET_STATUS: // CMD_GET_STATUS
        response[0] = CMD_RESP_GET_STATUS_ACK;
        memcpy(response+1, &gps_operation_result, 4);
        Serial.write(response,5);
        break;

    case CMD_SET_RADIO_DEFAULTS: // SET RADIO DEFAULTS
        set_radio_defaults();
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_RESTORE_RADIO_DEFAULTS: // RESTORE RADIO DEFAULTS
        restore_radio_defaults();
        response[0] = CMD_RESP_ACK;
        Serial.write(response,1);
        break;

    case CMD_RADIO_RESET: // RADIO RESET
        resetFunc();
        break;

    default:
        response[0] = CMD_RESP_WRONG_COMMAND;
        Serial.write(response, 1);
    }

}

int catCount = 0;
void checkCAT(){
  byte i;

  //Check Serial Port Buffer
  if (Serial.available() == 0) {      //Set Buffer Clear status
    rxBufferCheckCount = 0;
    return;
  }
  else if (Serial.available() < 5) {                         //First Arrived
      if (rxBufferCheckCount == 0){
          rxBufferCheckCount = Serial.available();
          rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
      }
      else if (rxBufferArriveTime < millis()){                //Clear Buffer
          for (i = 0; i < Serial.available(); i++)
              rxBufferCheckCount = Serial.read();
          rxBufferCheckCount = 0;
      }
      else if (rxBufferCheckCount < Serial.available()){      // Increase buffer count, slow arrive
          rxBufferCheckCount = Serial.available();
          rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
      }
      return;
  }

  //Arived CAT DATA
  for (i = 0; i < 5; i++)
      cat[i] = Serial.read();

  catCount++;

  processCATCommand(cat);
}
