#pragma once
#include "page5taskpayload.h"
#include "tasksettings.h"

struct TransientRelayConditions {
    bool needsShort, needsDischarge;
    QVector<QString> shortMask, dischargeMask;
};

// Pure validation shared by whole-run preflight and execution. No device access.
inline TransientRelayConditions transientRelayConditions(const Page5ExecutionContext &context,
                                                         Page5TaskKind::Kind kind,
                                                         const TaskSettings::Transient &settings)
{
    using Kind = Page5TaskKind::Kind;
    const auto single = Page5TaskKind::singleKind(kind);
    const bool shorted = single == Kind::OnShort || single == Kind::ShortOn;
    const bool discharge = single == Kind::TurnOn || shorted;
    TransientRelayConditions result{shorted, discharge, {}, {}};
    if (!shorted && !discharge) return result;
    if (context.page1.relayOutputs <= 0)
        throw std::invalid_argument("Select relay conditions with mapped ON outputs (short/discharge)");
    result.shortMask.fill("OFF", context.page1.relayOutputs);
    result.dischargeMask = result.shortMask;
    auto read = [&](int index, bool required, QVector<QString> &mask) {
        if (!required) return;
        if (index < 0 || index >= context.relays.size() || context.relays[index].values.size() != mask.size())
            throw std::invalid_argument("Invalid short/discharge relay condition size or index");
        bool hasOn = false;
        for (int i = 0; i < mask.size(); ++i) {
            mask[i] = context.relays[index].values[i].trimmed().toUpper();
            if (mask[i] != "ON" && mask[i] != "OFF")
                throw std::invalid_argument("Relay condition must contain ON/OFF values");
            if (mask[i] != "ON") continue;
            hasOn = true;
            bool mapped = false;
            for (const auto &instrument : context.page1.instruments) {
                if (instrument.type != "Relay" || !instrument.enabled || instrument.getResourceString().isEmpty())
                    continue;
                for (const auto &channel : instrument.channels)
                    if (channel.index == i + 1) mapped = true;
            }
            if (!mapped) throw std::invalid_argument("Short/discharge relay ON output is not mapped");
        }
        if (!hasOn) throw std::invalid_argument("Select relay conditions with mapped ON outputs (short/discharge)");
    };
    read(settings.relay, shorted, result.shortMask);
    read(settings.discharge, discharge, result.dischargeMask);
    for (int i = 0; i < result.shortMask.size(); ++i)
        if (result.shortMask[i] == "ON" && result.dischargeMask[i] == "ON")
            throw std::invalid_argument("Short and discharge must use different relay outputs");
    return result;
}
