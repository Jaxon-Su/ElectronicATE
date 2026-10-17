#pragma once
#include <QVariantMap>
#include <QStringList>
#include <cmath>
#include <limits>
#include <stdexcept>

// Independent TestConditions indices. In a measurement, -1 (None) requires that source OFF.
struct InputSelection {
    int ac = -1;
    int dc = -1;
    InputSelection() = default;
    explicit InputSelection(const QVariantMap &values)
        : ac(index(values, "ac_input_index")), dc(index(values, "dc_input_index")) {}
    bool valid(int acCount, int dcCount) const
    {
        return (ac >= 0 || dc >= 0) && ac >= -1 && ac < acCount && dc >= -1 && dc < dcCount;
    }
    static int index(const QVariantMap &values, const char *key)
    {
        const auto value = values.value(key, -1);
        const auto type = value.metaType().id();
        const double number = value.toDouble();
        if ((type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
             type != QMetaType::ULongLong && type != QMetaType::Double && type != QMetaType::Float) ||
            !std::isfinite(number) || std::floor(number) != number || number < -1 ||
            number > std::numeric_limits<int>::max())
            throw std::invalid_argument(QString("Invalid condition index: %1").arg(key).toStdString());
        return static_cast<int>(number);
    }
};

struct InputConditionOptions {
    QStringList ac;
    QStringList dc;
};
