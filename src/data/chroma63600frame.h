#pragma once
#include "instrumentsetupconfig.h"
#include "loadcapabilitycatalog.h"

namespace Chroma63600Frame {
inline int lastChannel(const QString &frame)
{
    if (frame == "63600-1") return 1;
    if (frame == "63600-2") return 3;
    if (frame == "63600-5") return 9;
    if (frame == "63601-5") return 10;
    return 0;
}

inline bool allows(const QString &frame, int channel, const QString &module)
{
    const int last = lastChannel(frame);
    if (!last) return true; // Not one of these four mainframes.
    if (channel < 1 || channel > last || !LoadCapabilityCatalog::Chroma63600Models.contains(module)) return false;
    return channel % 2 == 1 || (channel == 10 && module == "63610-80-20");
}

// Check physical channel numbers, not logical output indices. CH9 and CH10 share a slot,
// even if two Load rows refer to the same chassis address.
inline QString mappingError(const InstrumentSetupConfig &setup, const InstrumentConfig &load, int slot)
{
    if (!lastChannel(load.modelName)) return {};
    const int channel = load.channelNumbers.value(slot, -1);
    const QString &module = load.channels.at(slot).subModel;
    if (!allows(load.modelName, channel, module))
        return QString("%1: %2 is invalid on %3 CH%4").arg(load.name, module, load.modelName).arg(channel);
    if (load.modelName != "63601-5" || (channel != 9 && channel != 10)) return {};
    const int peer = channel == 9 ? 10 : 9;
    for (const auto &other : setup.instruments) {
        if (!other.enabled || other.type != "Load" || other.modelName != load.modelName ||
            other.getResourceString().compare(load.getResourceString(), Qt::CaseInsensitive) != 0) continue;
        for (int i = 0; i < other.channels.size(); ++i) {
            const auto &sibling = other.channels[i];
            if (other.channelNumbers.value(i, -1) == peer && !sibling.subModel.isEmpty() && sibling.subModel != module)
                return load.name + ": 63601-5 CH9 and CH10 share a physical module; CH10 requires 63610-80-20";
        }
    }
    return {};
}
} // namespace Chroma63600Frame
