#pragma once
#include "chromaload6310aspec.h"
#include "chromaload63600spec.h"
#include "conditionvalues.h"
#include "instrumentsetupconfig.h"
#include "loadcapabilitycatalog.h"

// Validate the whole selection before any channel is reprogrammed, including Change on live loads.
inline void validateManualLoadCurrents(const InstrumentSetupConfig &setup, const QVector<QString> &values,
                                       const QVector<QString> &ranges, const QVector<QString> &vo, bool dynamic)
{
    for (const auto &load : setup.instruments) {
        if (!load.enabled || load.type != "Load") continue;
        for (int i = 0; i < load.channels.size(); ++i) {
            const auto &channel = load.channels[i];
            const bool seriesA = LoadCapabilityCatalog::Chroma6310AModels.contains(channel.subModel);
            if (channel.index <= 0 || (!seriesA && !LoadCapabilityCatalog::Chroma63600Models.contains(channel.subModel))) continue;
            const int index = channel.index - 1;
            const QString label = QString("%1 CH%2 (%3)").arg(load.name).arg(load.channelNumbers.value(i)).arg(channel.subModel);
            const auto voltageText = vo.value(index).trimmed();
            double voltage = 0;
            if (!voltageText.isEmpty() && (!ConditionValues::finiteNumber(voltageText, voltage) || voltage < 0))
                throw std::invalid_argument((label + ": invalid Vo").toStdString());
            const auto levels = dynamic ? values.value(index).split('~') : QStringList{values.value(index)};
            if (levels.size() > 2) throw std::invalid_argument((label + ": invalid dynamic levels").toStdString());
            const QString range = ranges.value(index).trimmed().toUpper();
            for (const auto &level : levels) {
                double amps = 0;
                const bool parsed = dynamic ? ConditionValues::finiteNumber(level, amps)
                                            : ConditionValues::nonNegativeWithUnit(level, 'A', amps);
                if (!parsed || amps < 0) throw std::invalid_argument((label + ": invalid current").toStdString());
                if (seriesA) validateCurrent6310A(channel.subModel, load.channelNumbers.value(i), amps, voltage, range, dynamic);
                else validateCurrent63600(channel.subModel, amps, voltage, range, dynamic);
            }
        }
    }
}
