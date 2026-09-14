#pragma once
#include <QStringList>
#include <QSet>

namespace OutputIndexPolicy {
inline bool claim(const QString& index, QSet<QString>& used)
{
    if (index.isEmpty()) return true;
    if (used.contains(index)) return false;
    used.insert(index);
    return true;
}

inline bool isAvailable(const QString& option, const QString& current, const QSet<QString>& used)
{
    return option.isEmpty() || option == current || !used.contains(option);
}

inline bool isValid(int index, int outputCount)
{
    return index > 0 && index <= outputCount;
}

inline QStringList choices(int outputCount)
{
    QStringList result{QString()};
    for (int index = 1; index <= outputCount; ++index)
        result.append(QString::number(index));
    return result;
}

inline QString restoredChoice(const QString& previous, int outputCount)
{
    bool ok = false;
    const int index = previous.toInt(&ok);
    return ok && isValid(index, outputCount) ? QString::number(index) : QString();
}
}
