#pragma once
#include "page2config.h"
#include "conditionvalues.h"
#include <algorithm>
#include <cmath>
namespace ConditionPower {
inline bool parseMaxCurrentInRange(const QString& text, double& value)
{
    const QStringList parts = text.split(QRegularExpression("[~～]"), Qt::SkipEmptyParts);
    const QString currentText = parts.isEmpty() ? text : parts.last();
    return ConditionValues::nonNegativeWithUnit(currentText, 'A', value);
}

inline double load(const LoadMetaRow& meta, const QVector<QString>& rowVals, int outputCount)
{
    int N = std::min(outputCount, int(rowVals.size()));

    bool allEmpty = true;
    for (int i = 0; i < N; ++i) {
        if (!rowVals[i].trimmed().isEmpty()) {
            allEmpty = false;
            break;
        }
    }
    if (allEmpty)
        return std::nan("");

    double total = 0.0;
    bool hasPowerTerm = false;
    for (int i = 0; i < N; ++i) {
        const QString mode = (i < meta.modes.size() && !meta.modes[i].trimmed().isEmpty())
                                 ? meta.modes[i].trimmed().toUpper()
                                 : QStringLiteral("CC");

        if (mode == "CV") {
            double voltage = 0.0;
            double currentLimit = 0.0;
            if (!ConditionValues::cvPair(rowVals[i], voltage, currentLimit))
                continue;

            total += voltage * currentLimit;
            hasPowerTerm = true;
        } else {
            double value = 0.0;
            const bool valueOk = ConditionValues::nonNegativeWithUnit(rowVals[i], 'A', value);
            if (!valueOk)
                continue;

            if (i >= meta.vo.size())
                continue;
            bool voOk = false;
            const double vo = meta.vo[i].toDouble(&voOk);
            if (!voOk)
                continue;
            total += vo * value;
            hasPowerTerm = true;
        }
    }
    if (!hasPowerTerm)
        return std::nan("");
    return total;
}

inline double dynamic(const DynamicMetaRow& meta, const QVector<QString>& rowVals, int outputCount)
{
    int N = std::min(outputCount, int(rowVals.size()));

    double total = 0.0;
    bool hasPowerTerm = false;
    for (int i = 0; i < N; ++i) {
        double currentMax = 0.0;
        if (!parseMaxCurrentInRange(rowVals[i], currentMax))
            continue;

        if (i >= meta.vo.size())
            continue;
        bool voOk = false;
        const double vo = meta.vo[i].toDouble(&voOk);
        if (!voOk)
            continue;

        total += vo * currentMax;
        hasPowerTerm = true;
    }

    if (!hasPowerTerm)
        return std::nan("");
    return total;
}

} // namespace ConditionPower
