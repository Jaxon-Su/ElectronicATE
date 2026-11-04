#pragma once
#include "instrumentsetupconfig.h"
#include "loadcapabilitycatalog.h"

namespace Chroma6310Frame {
inline bool isFrame(const QString &frame) { return frame == "6314" || frame == "6314A"; }

inline int slotWidth(const QString &module)
{
    if (module == "63112" || module == "63112A") return 4;
    if (module == "63106" || module == "63106A" || module == "63108" || module == "63108A") return 2;
    return 1;
}

inline bool allows(const QString &frame, int channel, const QString &module)
{
    if (!isFrame(frame)) return true;
    const bool dual = module == "63102" || module == "63102A" || module == "63107" ||
                      module == "63107A" || module == "63110A";
    if (channel < 1 || channel > 8 || (channel % 2 == 0 && !dual) ||
        (channel - 1) / 2 + slotWidth(module) > 4) return false;
    if (frame == "6314A") return LoadCapabilityCatalog::Chroma6310AModels.contains(module);
    // Only the A modules with a corresponding legacy model are qualified on the old frame.
    const auto original = module.endsWith('A') ? module.chopped(1) : module;
    return LoadCapabilityCatalog::Chroma6310Models.contains(original);
}

inline bool canShareFrame(int channel, const QString &module, int otherChannel, const QString &otherModule)
{
    const int slot = (channel - 1) / 2, otherSlot = (otherChannel - 1) / 2;
    if (slot == otherSlot) return module == otherModule;
    return slot + slotWidth(module) <= otherSlot || otherSlot + slotWidth(otherModule) <= slot;
}

inline QString mappingError(const InstrumentSetupConfig &setup, const InstrumentConfig &load, int position)
{
    if (!isFrame(load.modelName)) return {};
    const int channel = load.channelNumbers.value(position, -1);
    const auto &module = load.channels[position].subModel;
    if (!allows(load.modelName, channel, module))
        return QString("%1: %2 is invalid on %3 CH%4").arg(load.name, module, load.modelName).arg(channel);
    for (const auto &other : setup.instruments) {
        if (!other.enabled || other.type != "Load" ||
            other.getResourceString().compare(load.getResourceString(), Qt::CaseInsensitive) != 0) continue;
        for (int i = 0; i < other.channels.size(); ++i) {
            const int peer = other.channelNumbers.value(i, -1);
            const auto &setting = other.channels[i];
            if (setting.subModel.isEmpty()) continue;
            if (other.modelName != load.modelName)
                return load.name + ": conflicting mainframe models at the same address";
            if (!allows(other.modelName, peer, setting.subModel) ||
                !canShareFrame(channel, module, peer, setting.subModel) ||
                (peer == channel && setting.index > 0 && other.name != load.name))
                return QString("%1 CH%2 conflicts with %3 CH%4 on the same mainframe")
                    .arg(load.name).arg(channel).arg(other.name).arg(peer);
        }
    }
    return {};
}
} // namespace Chroma6310Frame
