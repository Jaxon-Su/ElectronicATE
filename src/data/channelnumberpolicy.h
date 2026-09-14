#pragma once
#include <QList>
#include <QSet>
#include <QStringList>
#include <algorithm>

namespace ChannelNumberPolicy {
inline QSet<int> parse(const QStringList& values)
{
    QSet<int> result;
    for (const auto& value : values) {
        bool ok = false;
        const int number = value.toInt(&ok);
        if (ok) result.insert(number);
    }
    return result;
}

inline QList<int> sorted(const QSet<int>& numbers)
{
    auto result = numbers.values();
    std::sort(result.begin(), result.end());
    return result;
}

// Preserve the existing positional mapping and -1 for missing catalog slots.
inline QList<int> assign(int count, const QList<int>& catalog)
{
    QList<int> result;
    for (int i = 0; i < count; ++i)
        result.append(i < catalog.size() ? catalog[i] : -1);
    return result;
}
}
