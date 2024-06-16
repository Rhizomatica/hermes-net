// sBitx v3 ATTiny85 firmware
// Authors: Ashhar Farhan and Rafael Diniz

#include <Wire.h>
// #include <EEPROM.h>

int16_t fwd, ref;
byte message[4];

/*
** PB2 (SCL) and PB0 (SDA) are I2C
** PB1 is unused
** PB3 (vfwd) and PB4 (vref)
*/

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent()
{
    fwd = analogRead(PB3);
    ref = analogRead(PB4);
    message[0] = fwd & 0xff;
    message[1] = fwd >> 8;
    message[2] = ref & 0xff;
    message[3] = ref >> 8;
    Wire.write(message, 4); // 4 bytes message with fwd and ref
}

void setup()
{
    Wire.begin(8);                // join i2c bus with address #8
    Wire.onRequest(requestEvent); // register event

    // we could use PB1 for something
   //  pinMode(PB1, OUTPUT);
    pinMode(PB3, INPUT);
    pinMode(PB4, INPUT);


}

// bool blink = 0;
// uint16_t serial_number = 247;
// uint8_t low = serial_number & 0xff;
// uint8_t high = serial_number >> 8;
// EEPROM.write(0, low);
// EEPROM.write(1, high);

void loop()
{
//    digitalWrite(PB1, HIGH);
//    delay(5000);
//    digitalWrite(PB1, LOW);
//    delay(5000);
}
