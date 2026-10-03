#pragma once
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace ConditionValues {
inline bool finiteNumber(const QString &text, double &value)
{
    bool ok = false;
    value = text.toDouble(&ok);
    return ok && std::isfinite(value);
}

// Load values use optional base units only; this does not convert SI prefixes.
inline bool nonNegativeWithUnit(QString text, QChar unit, double &value)
{
    text = text.trimmed();
    text.remove(QRegularExpression(R"(\s+)"));
    if (text.endsWith(unit, Qt::CaseInsensitive))
        text.chop(1);
    return finiteNumber(text, value) && value >= 0;
}

inline bool cvPair(const QString &text, double &voltage, double &currentLimit)
{
    const auto parts = text.split('/', Qt::KeepEmptyParts);
    return parts.size() == 2 && nonNegativeWithUnit(parts[0], 'V', voltage) &&
           nonNegativeWithUnit(parts[1], 'A', currentLimit);
}

// Shared complete-input grammar; Qt validators also allow intermediate editing states.
inline QRegularExpression editorPattern(const QString &editor)
{
    return QRegularExpression(
            editor == "range"
                ? R"(^\d+(\.\d{0,10})?~\d+(\.\d{0,10})?$)"
                : R"(^\s*\d+(\.\d{0,10})?\s*([aA])?\s*(/\s*\d+(\.\d{0,10})?\s*([aA])?|[vV]\s*/\s*\d+(\.\d{0,10})?\s*([aA])?)?\s*$)");
}
} // namespace ConditionValues
