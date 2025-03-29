#include <Arduino.h>

void setup()
{
    // Pins

    // serial
    Serial.begin(19200, SERIAL_8N1); // input serial port (VE device)
    Serial.flush();
}

void loop()
{
    // Get data from sensors
    // Temperature
    float temperature = 0;

    // Charge controller stats

    // Screen Control

    // Wifi Control

    // Sleep
}
