#include "instrumentexecutor.h"

#include "relay.h"
#include "resourcecleaner.h"
#include <QScopeGuard>

namespace InstrumentExecutorInternal
{

void executeRelayAction(RelayBase *relay, RelayAction action, const InstrumentConfig &inst,
                        const ConditionLookup::RelayDataResult &dataInfo)
{
    if (!relay || !relay->isConnected())
        throw std::runtime_error("Relay is disconnected");

    const auto &states = dataInfo.targetStates;
    auto hardwareChannel = [&](int i) {
        return i < inst.channelNumbers.size() ? inst.channelNumbers[i] : i + 1;
    };
    // Applying a row must not partially execute an invalid configuration.
    if (action != RelayAction::RelayOff) {
        for (int i = 0; i < inst.channels.size(); ++i) {
            const int index = inst.channels[i].index;
            if (index == 0)
                continue;
            if (index < 0 || index > states.size())
                throw std::runtime_error("Relay condition channel index is out of range");
            const auto state = states[index - 1].trimmed().toUpper();
            if (state != "ON" && state != "OFF")
                throw std::runtime_error("Relay condition must be ON or OFF");
            if (hardwareChannel(i) < 1 || hardwareChannel(i) > relay->maxChannels())
                throw std::runtime_error("Relay hardware channel is out of range");
        }
    }
    QStringList errors;
    for (int i = 0; i < inst.channels.size(); ++i) {
        const int index = inst.channels[i].index;
        if (index == 0)
            continue;
        const int channel = hardwareChannel(i);
        try {
            if (channel < 1 || channel > relay->maxChannels())
                throw std::runtime_error("Relay hardware channel is out of range");
            // Cleanup opens every mapped hardware channel even if a condition index is stale.
            const bool on = action != RelayAction::RelayOff &&
                            states[index - 1].trimmed().compare("ON", Qt::CaseInsensitive) == 0;
            relay->setRealChannel(channel);
            const bool written = on ? relay->turnOn(channel) : relay->turnOff(channel);
            if (!written || relay->getStatus(channel) != (on ? RelayStatus::Closed : RelayStatus::Open))
                throw std::runtime_error("Relay write/readback failed");
        } catch (const std::exception &ex) {
            errors << QString("%1 channel %2: %3").arg(inst.name).arg(channel).arg(ex.what());
            if (action != RelayAction::RelayOff)
                break;
        }
    }
    if (!errors.isEmpty())
        throw std::runtime_error(errors.join("; ").toStdString());
}

} // namespace InstrumentExecutorInternal

InstrumentExecutor::Result InstrumentExecutor::runRelay(const Page1Config &cfg,
                                                        const QVector<RelayDataRow> &rows, int conditionIndex,
                                                        RelayAction action)
{
    using namespace InstrumentExecutorInternal;

    bool outputUnchanged = true;
    try {
        auto createResult = InstrumentCreator::createRelays(cfg);
        if (!createResult.success || createResult.relays().isEmpty())
            return {false, createResult.errorMessage.isEmpty()
                               ? "No Relay instrument configured or reachable in Page 1"
                               : createResult.errorMessage, true};

        auto dataInfo = ConditionLookup::findRelayData(rows, conditionIndex);
        if (!dataInfo.found) {
            return {false, "Relay data not found for index " + QString::number(conditionIndex), true};
        }

        bool hasError = !createResult.errorMessage.isEmpty();
        QString errorDetails = createResult.errorMessage;
        if (createResult.configurations().size() != createResult.relays().size() ||
            (hasError && action != RelayAction::RelayOff)) {
            return {false, "Relay initialization incomplete: " + errorDetails, true};
        }

        int relayIndex = 0;
        for (const auto &inst : createResult.configurations()) {
            RelayBase *relay = createResult.relays()[relayIndex++];

            if (relay && relay->isConnected()) {
                outputUnchanged = false;
                try {
                    executeRelayAction(relay, action, inst, dataInfo);
                } catch (const std::exception &e) {
                    hasError = true;
                    errorDetails += QString("\n- %1: %2").arg(inst.name).arg(e.what());
                }
            } else {
                hasError = true;
                errorDetails += "\nDisconnected relay: " + inst.name;
            }
        }

        if (hasError)
            return {false, "Some relay operations failed:" + errorDetails, outputUnchanged};
        return {};

    } catch (const std::exception &ex) {
        return {false, QString("[runRelay] Exception: %1").arg(ex.what()), outputUnchanged};
    }
}
