#include "instrumentexecutor.h"

#include "relay.h"
#include "resourcecleaner.h"

namespace InstrumentExecutorInternal {

void executeRelayAction(
    RelayBase* relay,
    RelayAction action,
    const InstrumentConfig& inst,
    const DataFinder::RelayDataResult& dataInfo)
{
    if (!relay || !relay->isConnected()) return;

    const QVector<QString>& targetStates = dataInfo.targetStates;

    for (int i = 0; i < inst.channels.size(); ++i) {
        int userIndex = inst.channels[i].index;
        int hwChannel = (i < inst.channelNumbers.size())
                            ? inst.channelNumbers[i]
                            : (i + 1);
        int dataIndex = userIndex - 1;
        if (dataIndex < 0 || dataIndex >= targetStates.size())
            throw std::runtime_error(
                QString("%1: channel index %2 is out of range (relay table has %3 column(s))")
                    .arg(inst.name).arg(userIndex).arg(targetStates.size()).toStdString());

        const QString stateStr = targetStates[dataIndex];
        const bool shouldBeOn  = (stateStr.compare("ON", Qt::CaseInsensitive) == 0);

        relay->setRealChannel(hwChannel);

        bool success = true;
        if (action == RelayAction::RelayOff) {
            success = relay->turnOff(hwChannel);
        } else {
            success = shouldBeOn ? relay->turnOn(hwChannel) : relay->turnOff(hwChannel);
        }

        if (success && (action == RelayAction::RelayOff || !shouldBeOn))
            success = relay->getStatus(hwChannel) == RelayStatus::Open;

        if (!success) {
            throw std::runtime_error(
                QString("Control failed for %1 Output %2")
                    .arg(inst.name).arg(userIndex).toStdString());
        }
    }
}

} // namespace InstrumentExecutorInternal

InstrumentExecutor::Result
InstrumentExecutor::runRelay(
    const Page1Config&           cfg,
    const QVector<RelayDataRow>& rows,
    int                          conditionIndex,
    RelayAction                  action)
{
    using namespace InstrumentExecutorInternal;

    try {
        auto createResult = InstrumentCreator::createRelays(cfg, nullptr);
        if (!createResult.success || createResult.relays.isEmpty())
            return { false, createResult.errorMessage.isEmpty()
                            ? "No Relay instrument configured or reachable in Page 1"
                            : createResult.errorMessage };

        auto dataInfo = DataFinder::findRelayData(rows, conditionIndex);
        if (!dataInfo.found) {
            ResourceCleaner::cleanupRelays(createResult.relays, createResult.commMap);
            return { false, "Relay data not found for index " + QString::number(conditionIndex) };
        }

        bool hasError = false;
        QString errorDetails;

        int relayIndex = 0;
        for (const auto& inst : cfg.instruments) {
            if (inst.type != "Relay" || !inst.enabled || inst.getResourceString().isEmpty()) continue;

            if (relayIndex >= createResult.relays.size()) break;
            RelayBase* relay = createResult.relays[relayIndex++];

            if (relay && relay->isConnected()) {
                try {
                    executeRelayAction(relay, action, inst, dataInfo);
                } catch (const std::exception& e) {
                    hasError = true;
                    errorDetails += QString("\n- %1: %2").arg(inst.name).arg(e.what());
                }
            }
        }

        ResourceCleaner::cleanupRelays(createResult.relays, createResult.commMap);

        if (hasError)
            return { false, "Some relay operations failed:" + errorDetails };
        return {};

    } catch (const std::exception& ex) {
        return { false, QString("[runRelay] Exception: %1").arg(ex.what()) };
    }
}
