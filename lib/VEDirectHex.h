#ifndef VEDIRECTHEX_H
#define VEDIRECTHEX_H

#include <Arduino.h>
// adapted from https://github.com/hoylabs/OpenDTU-OnBattery/tree/development/lib/VeDirectFrameHandler

#define VE_MAX_VALUE_LEN 33 // VE.Direct Protocol: max value size is 33 including /0
#define VE_MAX_HEX_LEN 100  // Maximum size of hex frame - max payload 34 byte (=68 char) + safe buffer

// https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf
enum VeDirectHexCommand
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

enum VeDirectHexRegister
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

#define ascii2hex(v) (v - 48 - (v >= 'A' ? 7 : 0))
#define hex2byte(b) (ascii2hex(*(b))) * 16 + ((ascii2hex(*(b + 1))))
static uint8_t calcHexFrameCheckSum(const char *buffer, const int size)
{
    uint8_t checksum = 0x55 - ascii2hex(buffer[1]);
    for (int i = 2; i < size; i += 2)
        checksum -= hex2byte(buffer + i);
    return (checksum);
}

String Int2HexLEString(const uint32_t value, const uint8_t anz)
{
    const char hexchar[] = "0123456789ABCDEF";
    char help[9] = {};

    switch (anz)
    {
    case 1:
        help[0] = hexchar[(value & 0x0000000F)];
        break;
    case 2:
    case 4:
    case 8:
        for (uint8_t i = 0; i < anz; i += 2)
        {
            help[i] = hexchar[(value >> ((1 + 1 * i) * 4)) & 0x0000000F];
            help[i + 1] = hexchar[(value >> ((1 * i) * 4)) & 0x0000000F];
        }
    default:;
    }
    return String(help);
}

static uint32_t AsciiHexLE2Int(const char *ascii, const uint8_t anz)
{
    char help[9] = {};

    // sort from little endian format to normal format
    switch (anz)
    {
    case 1:
        help[0] = ascii[0];
        break;
    case 2:
    case 4:
    case 8:
        for (uint8_t i = 0; i < anz; i += 2)
        {
            help[i] = ascii[anz - i - 2];
            help[i + 1] = ascii[anz - i - 1];
        }
    default:
        break;
    }
    return (static_cast<uint32_t>(strtoul(help, nullptr, 16)));
}

enum valueSize : uint8_t
{
    _8 = 8,
    _16 = 16,
    _32 = 32,
};

bool sendHexCommand(Stream &serial, const VeDirectHexCommand cmd, const VeDirectHexRegister addr, const uint32_t value, const valueSize valsize)
{
    bool sent = false;
    const uint8_t flags = 0x00; // always 0x00

    String txData = ":" + Int2HexLEString(static_cast<uint32_t>(cmd), 1); // add the command nibble

    using Command = VeDirectHexCommand;
    switch (cmd)
    {
    case Command::ping:
    case Command::version:
    case Command::id:
        sent = true;
        break;
    case Command::get:
    case Command::async:
        txData += Int2HexLEString(static_cast<uint16_t>(addr), 4);
        txData += Int2HexLEString(flags, 2); // add the flags (2 nibble)
        sent = true;
        break;
    case Command::set:
        txData += Int2HexLEString(static_cast<uint16_t>(addr), 4);
        txData += Int2HexLEString(flags, 2); // add the flags (2 nibble)
        if ((valsize == 8) || (valsize == 16) || (valsize == 32))
        {
            txData += Int2HexLEString(value, valsize / 4); // add value (2-8 nibble)
            sent = true;
        }
        break;
    default:
        sent = false;
        break;
    }

    if (sent)
    {
        // add the checksum (2 nibble)
        txData += Int2HexLEString(calcHexFrameCheckSum(txData.c_str(), txData.length()), 2);
        String send = txData + "\n"; // hex command end byte
        serial.write(send.c_str(), send.length());
    }

    return sent;
}

// Response
enum class VeDirectHexResponse : uint8_t
{
    nil = 0x0,
    DONE = 0x1,
    UNKNOWN = 0x3,
    ERROR = 0x4,
    PING = 0x5,
    GET = 0x7,
    SET = 0x8,
    ASYNC = 0xA
};
struct VeDirectHexData
{
    uint32_t value;           // integer value of register
    VeDirectHexRegister addr; // register address
    VeDirectHexResponse rsp;  // hex response code
    // uint8_t flags;            // flags Do the flags do anything?

    char text[VE_MAX_HEX_LEN]; // text/string response
};

VeDirectHexData disassembleHexData(const char buffer[VE_MAX_HEX_LEN])
{
    auto len = strlen(buffer);

    // reset hex data first
    VeDirectHexData data = {};

    if ((len > 3) && (calcHexFrameCheckSum(buffer, len) == 0x00))
    {
        data.rsp = static_cast<VeDirectHexResponse>(AsciiHexLE2Int(buffer + 1, 1));

        using Response = VeDirectHexResponse;
        switch (data.rsp)
        {
        case Response::DONE:
        case Response::ERROR:
        case Response::PING:
        case Response::UNKNOWN:
            strncpy(data.text, buffer + 2, len - 4);
            break;
        case Response::GET:
        case Response::SET:
        case Response::ASYNC:
            data.addr = static_cast<VeDirectHexRegister>(AsciiHexLE2Int(buffer + 2, 4));

            // future option: to analyse the flags here?
            // data.flags = AsciiHexLE2Int(buffer + 6, 2);

            if (len == 12)
            { // 8bit value
                data.value = AsciiHexLE2Int(buffer + 8, 2);
            }

            if (len == 14)
            { // 16bit value
                data.value = AsciiHexLE2Int(buffer + 8, 4);
            }

            if (len == 18)
            { // 32bit value
                data.value = AsciiHexLE2Int(buffer + 8, 8);
            }
            break;
        default:
            break; // something went wrong
        }
    }

    return data;
}

#endif