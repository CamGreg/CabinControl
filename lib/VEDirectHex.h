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

enum class VEDirectResponses
{
    done = 0x1,
    unknown = 0x3,
    error = 0x4,
    ping = 0x5,
    get = 0x7,
    set = 0x8,
};

enum VeDirectHexRegister : uint16_t
{
    chargerState = 0x0201,
    chargerError = 0xEDDA,
    chargeCurrent = 0xEDD7,
    chargeVoltage = 0xEDD5,
    batteryCurrent = 0xED8F,
    batteryVoltage = 0xED8D,
    userYield = 0xEDDC,
    yieldToday = 0xEDD3,
    peakToday = 0xEDD2,
    yieldYesterday = 0xEDD1,
    peakYesterday = 0xEDD0,
    panelPower = 0xEDBC,
    panelVoltage = 0xEDBB,
    panelCurrent = 0xEDBD,
    trackerMode = 0xEDB3,
    loadCurrent = 0xEDAD,
    loadVoltage = 0xEDA9,
    loadState = 0xEDA8,
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

static bool isHexDigit(char digit)
{
    return (digit >= '0' && digit <= '9') || (digit >= 'A' && digit <= 'F');
}

static uint8_t hexNibble(char digit)
{
    return digit <= '9' ? digit - '0' : digit - 'A' + 10;
}

static uint8_t calcHexFrameCheckSum(const char *buffer, const int size)
{
    uint8_t checksum = 0x55 - hexNibble(buffer[1]);
    for (int i = 2; i < size; i += 2)
        checksum -= (hexNibble(buffer[i]) << 4) | hexNibble(buffer[i + 1]);
    return checksum;
}

inline String Int2HexLEString(const uint32_t value, const uint8_t anz)
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

inline bool sendHexCommand(Stream &serial, const VeDirectHexCommand cmd, const VeDirectHexRegister addr, const uint32_t value, const valueSize valsize)
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
    case Command::restart:
        sent = true;
        break;
    case Command::get:
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
        return serial.write(send.c_str(), send.length()) == send.length();
    }

    return false;
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
    uint8_t flags;
    uint8_t payloadBytes;

    char text[VE_MAX_HEX_LEN]; // text/string response
};

inline VeDirectHexData disassembleHexData(const char *buffer, const int len)
{
    VeDirectHexData data = {};

    if (len < 4 || len > VE_MAX_HEX_LEN || len % 2 != 0 || buffer[0] != ':')
        return data;
    for (int i = 1; i < len; ++i)
    {
        if (!isHexDigit(buffer[i]))
            return data;
    }

    if (calcHexFrameCheckSum(buffer, len) == 0)
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
            if (len < 10)
                return {};
            data.addr = static_cast<VeDirectHexRegister>(AsciiHexLE2Int(buffer + 2, 4));
            data.flags = AsciiHexLE2Int(buffer + 6, 2);
            data.payloadBytes = (len - 10) / 2;

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
            return {};
        }
    }

    return data;
}

inline bool GetValue(Stream &serial, VeDirectHexRegister address, valueSize size, uint32_t &value)
{
    if (size != _8 && size != _16 && size != _32)
        return false;

    // Discard bytes buffered before this request, including replies from a previous poll.
    for (int pending = serial.available(); pending > 0; --pending)
        serial.read();
    if (!sendHexCommand(serial, VeDirectHexCommand::get, address, 0, size))
        return false;

    constexpr uint32_t responseTimeoutMs = 50;
    const uint32_t start = millis();
    char frame[VE_MAX_HEX_LEN];
    int length = 0;
    while (static_cast<uint32_t>(millis() - start) < responseTimeoutMs)
    {
        const int byte = serial.read();
        if (byte < 0)
        {
            delay(1);
            continue;
        }
        if (byte == ':')
        {
            frame[0] = ':';
            length = 1;
            continue;
        }
        if (length == 0)
            continue;
        if (byte == '\n')
        {
            const auto response = disassembleHexData(frame, length);
            length = 0;
            if (response.rsp != VeDirectHexResponse::GET || response.addr != address)
                continue;
            if (response.flags != 0)
                return false;
            if (response.payloadBytes == size / 8)
            {
                value = response.value;
                return true;
            }
            continue;
        }
        if (length == VE_MAX_HEX_LEN || !isHexDigit(byte))
        {
            length = 0;
            continue;
        }
        frame[length++] = static_cast<char>(byte);
    }
    return false;
}

inline uint32_t GetValue(Stream &serial, VeDirectHexRegister address, valueSize size)
{
    uint32_t value = 0;
    GetValue(serial, address, size, value);
    return value;
}

#endif
