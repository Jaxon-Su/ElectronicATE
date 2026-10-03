#pragma once
#include "oscshareddata.h"
#include <QJsonValue>
#include <QMap>
#include <QRegularExpression>
#include <QVariant>
#include <cmath>
#include <optional>

namespace OscilloscopeSettingRules
{
inline constexpr double horizontalMinimum = 0, horizontalMaximum = 100;
inline constexpr double positionMinimum = -8, positionMaximum = 8;

inline QString field(QString key)
{
    const auto match =
        QRegularExpression("^ch[1-8]_(enabled|position|scale|coupling|termination|bandwidth)$").match(key);
    return match.hasMatch() ? match.captured(1) : key;
}
inline QStringList choices(const QString &key)
{
    using namespace OscSharedData;
    const QMap<QString, QStringList> lists{
        {"horz_mode", MSO456_HORZ_MODES},
        {"horz_config", MSO456_HORZ_CONFIGS},
        {"acq_mode", ACQ_MODES},
        {"coupling", MSO456_COUPLINGS},
        {"termination", MSO456_TERMINATIONS},
        {"trig_source", {"CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "CH8", "AUX", "AUXILIARY", "LINE"}},
        {"trig_edge", {"RISING", "FALLING", "BOTH", "RISE", "FALL", "EITHER"}}};
    auto values = lists.value(field(key));
    for (auto &value : values)
        value = value.toUpper();
    return values;
}
inline std::optional<double> quantity(const QJsonValue &input, const QString &kind)
{
    if (!input.isString())
        return {};
    const auto match = QRegularExpression(R"(^([+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*(.*)$)")
                           .match(input.toString().trimmed());
    if (!match.hasMatch())
        return {};
    QMap<QString, double> units{{"", 1}};
    if (kind == "time")
        units.insert({{"s", 1}, {"ms", 1e-3}, {"us", 1e-6}, {"µs", 1e-6}, {"ns", 1e-9}});
    else if (kind == "channel")
        units.insert({{"V", 1}, {"mV", 1e-3}, {"uV", 1e-6}, {"A", 1}, {"mA", 1e-3}, {"uA", 1e-6}});
    else if (kind == "frequency")
        units.insert({{"Hz", 1}, {"kHz", 1e3}, {"MHz", 1e6}, {"GHz", 1e9}});
    else if (kind == "rate")
        units.insert({{"S/s", 1}, {"kS/s", 1e3}, {"MS/s", 1e6}, {"GS/s", 1e9}});
    else if (kind == "count")
        units.insert({{"k", 1e3}, {"M", 1e6}, {"G", 1e9}});
    if (!units.contains(match.captured(2)))
        return {};
    bool ok = false;
    const double value = match.captured(1).toDouble(&ok) * units.value(match.captured(2));
    if (!ok || !std::isfinite(value) || value <= 0 || value >= 1e30 ||
        (kind == "count" && std::floor(value) != value))
        return {};
    return QString::number(value, 'g', 15).toDouble();
}

// Returns normalized values; XML validation and runtime decoding share this
// contract.
inline std::optional<QVariant> decode(const QString &key, const QJsonValue &input)
{
    const QString name = field(key);
    // Channel-only fields are valid only with an explicit channel prefix.
    if (name == key &&
        QStringList{"enabled", "position", "scale", "coupling", "termination", "bandwidth"}.contains(name))
        return {};
    if (QStringList{"ignore_basic", "ignore_horizontal", "ignore_acquire", "ignore_channels",
                    "ignore_trigger", "fastacq", "enabled"}
            .contains(name))
        return input.isBool() ? std::optional<QVariant>(input.toBool()) : std::nullopt;
    if (name == "horz_pos" || name == "position")
    {
        const double value = input.toDouble();
        const double low = name == "horz_pos" ? horizontalMinimum : positionMinimum;
        const double high = name == "horz_pos" ? horizontalMaximum : positionMaximum;
        if (!input.isDouble() || !std::isfinite(value) || value < low || value > high)
            return {};
        return value;
    }
    const auto allowed = choices(key);
    if (!allowed.isEmpty())
    {
        const auto value = input.toString().trimmed().toUpper();
        if (!input.isString() || !allowed.contains(value))
            return {};
        return value;
    }
    if (name == "bandwidth" && input.isString() &&
        input.toString().trimmed().compare("Full", Qt::CaseInsensitive) == 0)
        return QString("FULL");
    const QMap<QString, QString> kinds{{"timescale", "time"},
                                       {"sample_rate", "rate"},
                                       {"record_length", "count"},
                                       {"scale", "channel"},
                                       {"bandwidth", "frequency"}};
    if (kinds.contains(name))
    {
        if (auto value = quantity(input, kinds.value(name)))
            return *value;
    }
    return {};
}
} // namespace OscilloscopeSettingRules
