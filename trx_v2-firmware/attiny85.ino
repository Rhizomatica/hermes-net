// sBitx v3 ATTiny85 firmware
// Authors: Ashhar Farhan and Rafael Diniz

#include <Wire.h>

int16_t fwd, ref;
byte message[4];

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent()
{
    fwd = analogRead(A2);
    ref = analogRead(A3);
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
}

void loop()
{
}
