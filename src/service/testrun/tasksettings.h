#pragma once
#include "testsequencetaskkind.h"
#include "tasksettingrules.h"
#include "captureoptions.h"
#include "oscilloscopesettings.h"
#include "groupconditionselection.h"
#include "brownoutsettings.h"
#include "inputselection.h"
#include "descentsettings.h"
#include <optional>
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
    InputSelection inputs;
    int load, settleMs;
    Direction searchDirection;
    bool captureEnabled;
    Capture captureSettings;
    explicit Measurement(const QVariantMap &values, bool dynamic = false)
        : inputs(values),
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
    int powerOffMs, startupMs;
    explicit Steady(const QVariantMap &values, bool dynamic = false)
        : Measurement(values, dynamic), autoPeriod(boolean(values, "auto_period")),
          powerOffMs(bounded(values, TaskSettingRules::powerOffWait)),
          startupMs(bounded(values, TaskSettingRules::startupWait))
    {
    }
};
struct Transient : Measurement {
    int relay, discharge, triggerTimeoutMs, phaseTimeoutMs, dischargeMs, trialsPerLevel;
    std::optional<DescentSettings::Settings> descent;
    explicit Transient(const QVariantMap &values, bool fixedDescent = false)
        : Measurement(values), relay(integer(values, "relay_index", -1)),
          discharge(integer(values, "discharge_index", -1)),
          triggerTimeoutMs(bounded(values, TaskSettingRules::triggerTimeout)),
          phaseTimeoutMs(bounded(values, TaskSettingRules::phaseTimeout)),
          dischargeMs(bounded(values, TaskSettingRules::discharge)),
          trialsPerLevel(fixedDescent ? bounded(values, "trials_per_level", 1, 1, 10)
                                     : bounded(values, TaskSettingRules::trials))
    {
        if (fixedDescent) descent.emplace(values);
    }
};
struct Group {
    TestSequenceTaskKind::Kind kind;
    std::variant<Steady, Transient> member;
    QVector<int> loads;

    Group(TestSequenceTaskKind::Kind taskKind, const QVariantMap &values)
        : kind(taskKind), member(TestSequenceTaskKind::isSteady(taskKind)
              ? std::variant<Steady, Transient>{Steady(values, TestSequenceTaskKind::singleKind(taskKind) == TestSequenceTaskKind::Kind::Dynamic)}
              : std::variant<Steady, Transient>{Transient(values)})
    {
        if (!TestSequenceTaskKind::isGroup(kind) ||
            !GroupConditionSelection::decode(values.value(TestSequenceTaskKind::loadKey(kind) + "_indices"), loads) || loads.isEmpty())
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
using Settings = std::variant<Delay, Relay, Steady, Group, Transient, WriteScope, BrownOut::Settings>;
struct PreparedTask {
    TestSequenceTaskKind::Kind kind;
    Settings settings;
};
inline PreparedTask decode(const QString &name, const QVariantMap &values, OscilloscopeSettings configuration = {})
{
    using Kind = TestSequenceTaskKind::Kind;
    const auto kind = TestSequenceTaskKind::fromName(name);
    switch (kind) {
    case Kind::BrownOut:
        return {kind, BrownOut::Settings(values)};
    case Kind::Delay:
        return {kind, Delay{bounded(values, TaskSettingRules::settle)}};
    case Kind::Relay:
        return {kind, Relay{integer(values, "relay_index", -1)}};
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
    case Kind::TurnOffDescent:
    case Kind::OnShortDescent:
        return {kind, Transient(values, true)};
    case Kind::WriteScope:
        return {kind, WriteScope{std::move(configuration)}};
    default:
        throw std::invalid_argument("Unknown task");
    }
}
} // namespace TaskSettings
