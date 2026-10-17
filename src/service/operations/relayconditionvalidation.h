#pragma once
#include "instrumentsetupconfig.h"
#include "testconditionsconfig.h"
#include <QSet>

// Pure validation for applying a condition; emergency OFF must not depend on it.
inline QString validateRelayCondition(const InstrumentSetupConfig &config, const QVector<RelayDataRow> &rows, int index)
{
    if (index < 0 || index >= rows.size())
        return "Selected relay row is out of range";
    const auto &values = rows[index].values;
    QSet<int> mapped;
    for (const auto &instrument : config.instruments) {
        if (!instrument.enabled || instrument.type != "Relay" || instrument.modelName.trimmed().isEmpty() ||
            instrument.getResourceString().trimmed().isEmpty())
            continue;
        for (const auto &channel : instrument.channels) {
            if (channel.index == 0) continue;
            if (channel.index < 0 || channel.index > values.size())
                return "Relay condition channel index is out of range";
            mapped.insert(channel.index);
        }
    }
    for (int i = 0; i < values.size(); ++i) {
        const QString state = values[i].trimmed().toUpper();
        if (state != "ON" && state != "OFF")
            return "Relay condition must be ON or OFF";
        if (state == "ON" && !mapped.contains(i + 1))
            return QString("Relay ON output %1 is not mapped to an enabled instrument").arg(i + 1);
    }
    return {};
}
