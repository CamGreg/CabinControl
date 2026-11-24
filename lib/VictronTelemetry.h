#pragma once

#include "VEDirectHex.h"

struct VictronField
{
    constexpr VictronField(const char *column, VeDirectHexRegister address, valueSize width,
                           int scale = 1, bool signedValue = false)
        : column(column), address(address), width(width), scale(scale), signedValue(signedValue)
    {
    }

    const char *column;
    VeDirectHexRegister address;
    valueSize width;
    int scale;
    bool signedValue;
};

// Keep the dashboard columns and units consistent with existing daily logs.
constexpr VictronField victronFields[] = {
    {"V", batteryVoltage, _16, 10, true},
    {"I", batteryCurrent, _16, 100, true},
    {"VPV", panelVoltage, _16, 10},
    {"PPV", panelPower, _32},
    {"IL", loadCurrent, _16, 100},
    {"H19", userYield, _32},
    {"H20", yieldToday, _16},
    {"H21", peakToday, _16},
    {"H22", yieldYesterday, _16},
    {"H23", peakYesterday, _16},
    {"CS", chargerState, _8},
    {"ERR", chargerError, _8},
    {"MPPT", trackerMode, _8},
    {"LOAD", loadState, _8},
};
constexpr size_t victronCount = sizeof(victronFields) / sizeof(*victronFields);
constexpr int64_t missingVictron = INT64_MIN;

inline void pollVictron(Stream &serial, int64_t (&values)[victronCount])
{
    for (size_t i = 0; i < victronCount; ++i)
    {
        const auto &field = victronFields[i];
        uint32_t raw;
        values[i] = missingVictron;
        if (GetValue(serial, field.address, field.width, raw))
        {
            if (field.address != chargerState || raw != 255)
                values[i] = (field.signedValue ? int64_t(int16_t(raw)) : int64_t(raw)) * field.scale;
            continue;
        }

        // Older models expose charger voltage/current and 32-bit daily yields.
        if (field.address == batteryVoltage && GetValue(serial, chargeVoltage, _16, raw))
            values[i] = int64_t(raw) * field.scale;
        if ((field.address == yieldToday || field.address == yieldYesterday) &&
            GetValue(serial, field.address, _32, raw))
            values[i] = raw;
    }
    if (values[1] == missingVictron && values[4] != missingVictron)
    {
        uint32_t raw;
        if (GetValue(serial, chargeCurrent, _16, raw))
            values[1] = int64_t(raw) * 100 - values[4];
    }
}
