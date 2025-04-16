#ifndef VEDIRECTHEX_H
#define VEDIRECTHEX_H

#include <Arduino.h>

// https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf
enum VEDirectCommands
{
    boot = 0x00,
    ping = 0x01,
    version = 0x03,
    id = 0x04,
    restart = 0x06,
    get = 0x07,
    set = 0x08,
    async = 0x0A,
};

enum VEDirectResponses
{
    done = 0x1,
    unknown = 0x3,
    error = 0x4,
    ping = 0x5,
    get = 0x7,
    set = 0x8,
};

enum VEDirectAddresses
{
    chargeCurrent = 0xEDD7,
    chargeVoltage = 0xEDD5,
    panelPower = 0xEDBC,
    panelVoltage = 0xEDBB,
    panelCurrent = 0xEDBD,
    loadCurrent = 0xEDAD,
    loadVoltage = 0xEDA9,
    batteryMaximumCurrent = 0xEDF0,
};

// The frame format of the VE.Direct protocol has the following general format:
// :[command][data][data][…][check]\n
// Where the colon indicates the start of the frame and the newline is the end of frame. The sum of all
// data bytes and the check must equal 0x55.
// Since the normal protocol is in text values the frames are
// sent in their hexadecimal ASCII representation, [‘0’ .. ’9’], [‘A’ .. ’F’], must be uppercase. There is no
// need to escape any characters.

// : [command] [dataHighNibble, dataLowNibble][……] [checkHigh, checkLow] \n

// Note: The command is only send as a single nibble. Numbers are sent in Little Endian format. An
// error response with value 0xAAAA is sent on framing errors.

// ex:
// Get Battery Maximum Current
// :7F0ED0071\n       :[7][F0ED][00][71]\n
// :7F0ED009600DB\n   :[7][F0ED][00][9600][DB]\n
// Value = 0x0096 = 15.0A

uint32_t Get(Stream &serial, int16_t address, int8_t flags)
{
    serial.print(F(":"));
    serial.write(VEDirectCommands::get);
    serial.write(address >> 8);
    serial.write(address & 0xFF);
    serial.write(flags);
    serial.write(0x55);
    serial.println();

    // Read response
    uint8_t response[33];
    serial.setTimeout(200); // TODO fine tune this
    serial.readBytesUntil('\n', response, 33);

    // TODO Parse response

    return 0;
}

#endif