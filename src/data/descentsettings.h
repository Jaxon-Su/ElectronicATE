#pragma once
#include <QVariantMap>
#include <QVector>
#include <cmath>
#include <stdexcept>

namespace DescentSettings {
inline constexpr int maximumLevels = 1000;
inline constexpr double maximumMagnitude = 1e9; // Numeric editor bound, not an instrument rating.
inline constexpr int levelDecimals = 4;
inline bool validLevel(double value)
{
    // Compare a decimal roundtrip, not a tolerance that can accept extra digits.
    return std::isfinite(value) && std::abs(value) <= maximumMagnitude &&
           QString::number(value, 'f', levelDecimals).toDouble() == value;
}
inline QString levelText(double value)
{
    if (!validLevel(value)) return QString::number(value, 'g', 17); // Preserve invalid drafts for correction.
    QString text = QString::number(value, 'f', levelDecimals);
    while (text.endsWith('0')) text.chop(1);
    if (text.endsWith('.')) text.chop(1);
    return text == "-0" ? QStringLiteral("0") : text;
}
inline bool numeric(const QVariant &value)
{
    const auto type = value.metaType().id();
    return type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::LongLong ||
           type == QMetaType::ULongLong || type == QMetaType::Double || type == QMetaType::Float;
}
inline bool validChannel(const QVariant &value)
{
    const double n = value.toDouble();
    return numeric(value) && std::isfinite(n) && n >= 1 && n <= 8 && std::floor(n) == n;
}
inline bool decodeLevels(const QVariant &value, QVector<double> &levels, bool allowEmpty = false)
{
    levels.clear();
    if (value.metaType().id() != QMetaType::QVariantList) return false;
    const auto list = value.toList();
    if ((!allowEmpty && list.isEmpty()) || list.size() > maximumLevels) return false;
    for (const auto &entry : list) {
        const double n = entry.toDouble();
        if (!numeric(entry) || !validLevel(n)) return false;
        levels.append(n); // Preserve order and repeated levels.
    }
    return true;
}
inline QVariantMap defaults()
{
    return {{"trigger_channel", 4}, {"trigger_levels", QVariantList{15.0, 10.0, 5.0}},
            {"trials_per_level", 1}};
}
struct Settings {
    int channel = 0;
    bool channelEnabled = true;
    QVector<double> levels;
    explicit Settings(const QVariantMap &values, bool allowChannelEnable = false)
    {
        if (values.contains("trigger_channel_enabled")) {
            if (!allowChannelEnable || values.value("trigger_channel_enabled").metaType().id() != QMetaType::Bool)
                throw std::invalid_argument("Descent: invalid trigger_channel_enabled setting");
            channelEnabled = values.value("trigger_channel_enabled").toBool();
        }
        if (!validChannel(values.value("trigger_channel")))
            throw std::invalid_argument("Descent: select Trigger CH1..CH8");
        channel = values.value("trigger_channel").toInt();
        if (!decodeLevels(values.value("trigger_levels"), levels))
            throw std::invalid_argument("Descent: enter Trigger levels within +/-1e9, with at most four decimal places");
    }
};
}
