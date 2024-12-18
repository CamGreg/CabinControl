# Cabin Control

This is a project to control a backwoods cabin. It's main purpose is to record the power usage.

## Hardware

The software componets are designed to be run on a ESP32-C3-DevKitC-02 board.

The power data is to be read from 2 [Victron Energy MPPT 75](https://www.victronenergy.com/upload/documents/Datasheet-SmartSolar-charge-controller-MPPT-75-10,-75-15,-100-15,-100-20_48V-EN.pdf)
charge controllers. Using their [VE.Direct Protocol](https://www.victronenergy.com/upload/documents/VE.Direct-Protocol-3.33.pdf) and port.
The port is 5V ([FAQ Q4](https://www.victronenergy.com/live/vedirect_protocol:faq)), so we need a level shifter to convert it to 3.3V for the ESP32.
The port is listed as TTL/RS232. The RS232 portion appear to be an indication that it is to be used with their RS232 cable, however the TTL part appears to be what it actually outputs.
This is assumed to be UART so we can use the ESP32's UART port.
It is not an addressed protocol and the ESP32 only has one UART port, so we will need to add hardware to select between the two controllers.
The level shifter could easily be chosen to have an enable pin to select between the two controllers.
