#pragma once
#include <QSet>
#include <QVariant>
#include <QVector>
#include <cmath>
#include <limits>

namespace GroupConditionSelection {
// Empty lists are editable drafts. Execution additionally requires configured conditions.
inline bool decode(const QVariant &value, QVector<int> &indices)
{
    indices.clear();
    if (value.metaType().id() != QMetaType::QVariantList)
        return false;
    QSet<int> seen;
    for (const auto &entry : value.toList()) {
        const auto type = entry.metaType().id();
        if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
            type != QMetaType::ULongLong && type != QMetaType::Double && type != QMetaType::Float)
            return false;
        const double number = entry.toDouble();
        if (!std::isfinite(number) || number < 0 || number > std::numeric_limits<int>::max() ||
            std::floor(number) != number || seen.contains(static_cast<int>(number)))
            return false;
        const int index = static_cast<int>(number);
        seen.insert(index);
        indices.append(index);
    }
    return true;
}
} // namespace GroupConditionSelection
