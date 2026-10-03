#pragma once
#include "page1config.h"

inline bool hasConfiguredAc(const Page1Config &config)
{
    for (const auto &instrument : config.instruments)
        if (instrument.enabled && instrument.type == "InputSource" && instrument.name == "ACSource")
            return true;
    return false;
}
inline bool hasConfiguredDc(const Page1Config &config)
{
    for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source)
        for (const auto &instrument : config.instruments)
            if (instrument.enabled && instrument.type == "InputDCSource" &&
                instrument.name == QString("DC Source%1").arg(source + 1))
                return true;
    return false;
}
