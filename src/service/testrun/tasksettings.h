#pragma once
#include "page5taskkind.h"
#include "tasksettingrules.h"
#include "captureoptions.h"
#include "oscilloscopesettings.h"
#include "groupconditionselection.h"
#include <QVariantMap>
#include <QStringList>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <variant>
#include <algorithm>

namespace TaskSettings
{
enum class Direction { Both, Rise, Fall };
inline Direction direction(const QVariantMap &values)
{
    const auto text = values.value("measure_target", "BOTH").toString().trimmed().toUpper();
    if (text == "BOTH")
        return Direction::Both;
    if (text == "MAX")
        return Direction::Rise;
    if (text == "MIN")
        return Direction::Fall;
    throw std::invalid_argument("Invalid search direction");
}
inline QString serialized(Direction direction)
{
    switch (direction) {
    case Direction::Rise:
        return "MAX";
    case Direction::Fall:
        return "MIN";
    default:
        return "BOTH";
    }
}
inline int integer(const QVariantMap &values, const char *key, int fallback)
{
    if (!values.contains(key))
        return fallback;
    const auto type = values.value(key).metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
        type != QMetaType::ULongLong && type != QMetaType::Double && type != QMetaType::Float)
        throw std::invalid_argument(QString("Non-numeric setting: %1").arg(key).toStdString());
    bool ok = false;
    const double number = values.value(key).toDouble(&ok);
    if (!ok || !std::isfinite(number) || std::floor(number) != number ||
        number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
        throw std::invalid_argument(QString("Invalid integer setting: %1").arg(key).toStdString());
    return static_cast<int>(number);
}
using Capture = CaptureOptions;
inline int bounded(const QVariantMap &values, const char *key, int fallback, int minimum, int maximum)
{
    const int value = integer(values, key, fallback);
    if (value < minimum || value > maximum)
        throw std::invalid_argument(QString("Setting out of range: %1").arg(key).toStdString());
    return value;
}
inline int bounded(const QVariantMap &values, const TaskSettingRules::Integer &rule)
{
    return bounded(values, rule.key, rule.initial, rule.minimum, rule.maximum);
}
inline bool boolean(const QVariantMap &values, const char *key)
{
    if (!values.contains(key))
        return false;
    if (values.value(key).metaType().id() != QMetaType::Bool)
        throw std::invalid_argument(QString("Invalid boolean setting: %1").arg(key).toStdString());
    return values.value(key).toBool();
}
struct Measurement {
    int input, load, settleMs;
    Direction searchDirection;
    bool captureEnabled;
    Capture captureSettings;
    explicit Measurement(const QVariantMap &values, bool dynamic = false)
        : input(integer(values, "input_index", -1)),
          load(integer(values, dynamic ? "dyload_index" : "load_index", -1)),
          settleMs(bounded(values, TaskSettingRules::settle)), searchDirection(direction(values)),
          captureEnabled(boolean(values, "capture_enabled")),
          captureSettings(captureEnabled ? Capture(values.value("capture_settings").toMap(), true)
                                         : Capture{})
    {
    }
};
struct Steady : Measurement {
    bool autoPeriod;
    explicit Steady(const QVariantMap &values, bool dynamic = false)
        : Measurement(values, dynamic), autoPeriod(boolean(values, "auto_period"))
    {
    }
};
struct Transient : Measurement {
    int relay, discharge, triggerTimeoutMs, phaseTimeoutMs, dischargeMs, trialsPerLevel;
    explicit Transient(const QVariantMap &values)
        : Measurement(values), relay(integer(values, "relay_index", -1)),
          discharge(integer(values, "discharge_index", -1)),
          triggerTimeoutMs(bounded(values, TaskSettingRules::triggerTimeout)),
          phaseTimeoutMs(bounded(values, TaskSettingRules::phaseTimeout)),
          dischargeMs(bounded(values, TaskSettingRules::discharge)),
          trialsPerLevel(bounded(values, TaskSettingRules::trials))
    {
    }
};
struct Group {
    Page5TaskKind::Kind kind;
    std::variant<Steady, Transient> member;
    QVector<int> loads;

    Group(Page5TaskKind::Kind taskKind, const QVariantMap &values)
        : kind(taskKind), member(Page5TaskKind::isSteady(taskKind)
              ? std::variant<Steady, Transient>{Steady(values, Page5TaskKind::singleKind(taskKind) == Page5TaskKind::Kind::Dynamic)}
              : std::variant<Steady, Transient>{Transient(values)})
    {
        if (!Page5TaskKind::isGroup(kind) ||
            !GroupConditionSelection::decode(values.value(Page5TaskKind::loadKey(kind) + "_indices"), loads) || loads.isEmpty())
            throw std::invalid_argument("Select one or more distinct Load conditions");
        std::sort(loads.begin(), loads.end());
    }
    const Measurement &measurement() const
    {
        return std::visit([](const auto &settings) -> const Measurement& { return settings; }, member);
    }
};
struct Delay {
    int milliseconds;
};
struct Relay {
    int index;
};
struct WriteScope {
    OscilloscopeSettings configuration;
};
using Settings = std::variant<Delay, Relay, Capture, Steady, Group, Transient, WriteScope>;
struct PreparedTask {
    Page5TaskKind::Kind kind;
    Settings settings;
};
inline PreparedTask decode(const QString &name, const QVariantMap &values, OscilloscopeSettings configuration = {})
{
    using Kind = Page5TaskKind::Kind;
    const auto kind = Page5TaskKind::fromName(name);
    switch (kind) {
    case Kind::Delay:
        return {kind, Delay{bounded(values, TaskSettingRules::settle)}};
    case Kind::Relay:
        return {kind, Relay{integer(values, "relay_index", -1)}};
    case Kind::Capture:
        return {kind, Capture(values)};
    case Kind::Static:
        return {kind, Steady(values)};
    case Kind::Dynamic:
        return {kind, Steady(values, true)};
    case Kind::StaticGroup:
    case Kind::DynamicGroup:
    case Kind::TurnOnGroup:
    case Kind::TurnOffGroup:
    case Kind::OnShortGroup:
    case Kind::ShortOnGroup:
        return {kind, Group(kind, values)};
    case Kind::TurnOn:
    case Kind::TurnOff:
    case Kind::OnShort:
    case Kind::ShortOn:
        return {kind, Transient(values)};
    case Kind::WriteScope:
        return {kind, WriteScope{std::move(configuration)}};
    default:
        throw std::invalid_argument("Unknown task");
    }
}
} // namespace TaskSettings
