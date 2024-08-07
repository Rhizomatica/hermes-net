// sBitx v3 ATTiny85 firmware
// Authors: Ashhar Farhan and Rafael Diniz

#include <Wire.h>
#include <EEPROM.h>

/*
** PB2 (SCL) and PB0 (SDA) are I2C
** PB1 is unused
** PB3 (vfwd) and PB4 (vref)
*/

// cat commands
#define CMD_READ_PB1_DIGITAL 1
#define CMD_READ_PB1_ANALOG 2
#define CMD_SET_PB1 3
#define CMD_GET_SERIAL 4
#define CMD_SET_SERIAL 5
#define CMD_MEM_SET 6
#define CMD_MEM_GET 7
#define CMD_RADIO_RESET 8

// cat responses
#define CMD_RESP_ACK 1
#define CMD_RESP_READ_PB1_DIGITAL 2
#define CMD_RESP_READ_PB1_ANALOG 3
#define CMD_RESP_GET_SERIAL_ACK 4
#define CMD_RESP_MEM_FAIL 5
#define CMD_RESP_WRONG_COMMAND 6

#define CAT_RECEIVE_TIMEOUT 500
#define SERIAL_EEPROM_OFFSET 0

uint32_t rxBufferArriveTime = 0;
uint8_t rxBufferCheckCount = 0;

uint8_t cat[5];

uint8_t message[4];
bool response_available = false;
uint8_t response[4];

uint8_t pin_mode = INPUT;
uint32_t serial;

void(*reset) (void) = 0;

void run_cmd(byte* cmd)
{
    int16_t measure, offset;

    memset(response, 0, 4);

    switch(cmd[4]){

    case CMD_READ_PB1_DIGITAL: // Digital Read PB1
        if (pin_mode != INPUT)
        {
            pin_mode = INPUT;
            pinMode(PB1, pin_mode);
        }
        response[0] = CMD_RESP_READ_PB1_DIGITAL;
        measure = digitalRead(PB1);
        memcpy(response+1, &measure, 2);
        break;

    case CMD_READ_PB1_ANALOG: // Analog Read PB1
        if (pin_mode != INPUT)
        {
            pin_mode = INPUT;
            pinMode(PB1, pin_mode);
        }
        response[0] = CMD_RESP_READ_PB1_ANALOG;
        measure = analogRead(PB1);
        memcpy(response+1, &measure, 2);
        break;

    case CMD_SET_PB1: // SET PB1
        if (pin_mode != OUTPUT)
        {
            pin_mode = OUTPUT;
            pinMode(PB1, pin_mode);
        }
        digitalWrite(PB1, cmd[0]);
        response[0] = CMD_RESP_ACK;
        break;

    case CMD_GET_SERIAL: // GET SERIAL NUMBER
        response[0] = CMD_RESP_GET_SERIAL_ACK;
        memcpy(response+1, &serial, 3); // we allocate 24 bit for the serial
      break;

    case CMD_SET_SERIAL: // SET SERIAL NUMBER
        serial = 0;
        memcpy(&serial, cmd, 4);
        // write serial number (3 bytes) to EEPROM
        EEPROM.update(SERIAL_EEPROM_OFFSET, serial & 0xff);
        EEPROM.update(SERIAL_EEPROM_OFFSET+1, (serial >> 8) & 0xff) ;
        EEPROM.update(SERIAL_EEPROM_OFFSET+2, (serial >> 16) & 0xff) ;
        response[0] = CMD_RESP_ACK;
        break;

    case CMD_MEM_SET: //
        memcpy(&offset, cmd, 2);
        if (offset >= 512)
            response[0] = CMD_RESP_MEM_FAIL;
        else
        {
            memcpy(&measure, cmd, 2);
            EEPROM.update(offset, measure & 0xff);
            response[0] = CMD_RESP_ACK;
        }
        break;
    case CMD_MEM_GET: //
        memcpy(&offset, cmd, 2);
        if (offset >= 512)
            response[0] = CMD_RESP_MEM_FAIL;
        else
        {
            response[0] = CMD_RESP_ACK;
            response[1] = EEPROM.read(offset);
        }
        break;

    case CMD_RADIO_RESET: // RADIO RESET
        reset();
        break;

    default:
        response[0] = CMD_RESP_WRONG_COMMAND;
        break;
    }
    response_available = true;

}

// 5 bytes commands
void receiveEvent(int bytes)
{
    uint8_t i;

    if (Wire.available() == 0)
    {      //Set Buffer Clear status
        rxBufferCheckCount = 0;
        return;
    }
    else
        if (Wire.available() < 5)
        {                         //First Arrived
            if (rxBufferCheckCount == 0)
            {
                rxBufferCheckCount = Wire.available();
                rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
            }
            else
                if (rxBufferArriveTime < millis())
                {                //Clear Buffer
                    for (i = 0; i < Wire.available(); i++)
                        rxBufferCheckCount = Wire.read();
                    rxBufferCheckCount = 0;
                }
                else
                    if (rxBufferCheckCount < Wire.available())
                    {      // Increase buffer count, slow arrive
                        rxBufferCheckCount = Wire.available();
                        rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
                    }
            return;
        }


    //Arived CAT DATA
    for (i = 0; i < 5; i++)
        cat[i] = Wire.read();

    run_cmd(cat);
}


// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent()
{
    int16_t fwd, ref;

    if (response_available == true)
    {
        Wire.write(response, 4); // 4 bytes message with fwd and ref
        response_available = false;
    }
    else
    {
        fwd = analogRead(PB3);
        ref = analogRead(PB4);
        message[0] = fwd & 0xff;
        message[1] = fwd >> 8;
        message[2] = ref & 0xff;
        message[3] = ref >> 8;
        Wire.write(message, 4); // 4 bytes message with fwd and ref
    }
}

void setup()
{
    Wire.begin(8);                // join i2c bus with address #8
    Wire.onRequest(requestEvent); // register event
    Wire.onReceive(receiveEvent);

    // read serial number from EEPROM
    serial = 0;
    serial = (uint32_t) EEPROM.read(SERIAL_EEPROM_OFFSET);
    serial |= (uint32_t) EEPROM.read(SERIAL_EEPROM_OFFSET+1) << 8;
    serial |= (uint32_t) EEPROM.read(SERIAL_EEPROM_OFFSET+2) << 16;

    pinMode(PB1, pin_mode);
    pinMode(PB3, INPUT);
    pinMode(PB4, INPUT);
}

void loop()
{
}
