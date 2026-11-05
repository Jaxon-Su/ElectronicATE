#pragma once
#include "instrumentsetupconfig.h"
#include "loadcapabilitycatalog.h"

namespace Chroma63600Frame {
inline int lastChannel(const QString &frame)
{
    if (frame == "63600-1") return 2;
    if (frame == "63600-2") return 4;
    if (frame == "63600-5" || frame == "63601-5") return 10;
    return 0;
}

inline bool allows(const QString &frame, int channel, const QString &module)
{
    const int last = lastChannel(frame);
    if (!last) return true; // Not one of these four mainframes.
    if (channel < 1 || channel > last || !LoadCapabilityCatalog::Chroma63600Models.contains(module)) return false;
    return channel % 2 == 1 || (module == "63610-80-20" && (frame != "63601-5" || channel == 10));
}

// Each odd/even pair is one installed module, including across Load rows on the same chassis.
inline QString mappingError(const InstrumentSetupConfig &setup, const InstrumentConfig &load, int slot)
{
    const bool is63600 = lastChannel(load.modelName) != 0;
    const int channel = load.channelNumbers.value(slot, -1);
    const QString &module = load.channels.at(slot).subModel;
    if (is63600 && !allows(load.modelName, channel, module))
        return QString("%1: %2 is invalid on %3 CH%4").arg(load.name, module, load.modelName).arg(channel);
    const QString resource = load.getResourceString();
    const int peer = channel % 2 ? channel + 1 : channel - 1;
    int owners = 0;
    for (const auto &other : setup.instruments) {
        if (other.type != "Load" || (!is63600 && !lastChannel(other.modelName)) ||
            other.getResourceString().compare(resource, Qt::CaseInsensitive) != 0) continue;
        for (int i = 0; i < other.channels.size(); ++i) {
            const auto &sibling = other.channels[i];
            if (sibling.index <= 0 && sibling.subModel.isEmpty()) continue;
            if (other.modelName != load.modelName)
                return load.name + ": conflicting load frames at " + resource;
            const int physical = other.channelNumbers.value(i, -1);
            if (physical == channel && sibling.index > 0 && ++owners > (load.channels[slot].index > 0 ? 1 : 0))
                return QString("%1: duplicate hardware channel at %2 CH%3").arg(load.name, resource).arg(channel);
            if ((physical == channel || physical == peer) && !sibling.subModel.isEmpty() && sibling.subModel != module)
                return QString("%1: CH%2 and CH%3 share one module at %4").arg(load.name).arg(channel).arg(physical).arg(resource);
        }
    }
    return {};
}
} // namespace Chroma63600Frame
