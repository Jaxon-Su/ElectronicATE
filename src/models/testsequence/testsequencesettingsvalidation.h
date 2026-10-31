#pragma once
#include "testsequencetaskkind.h"
#include "tasksettingrules.h"
#include "oscilloscopesettingrules.h"
#include "groupconditionselection.h"
#include "brownoutsettings.h"
#include "descentsettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>

namespace TestSequenceSettingsValidation
{
inline bool retryCount(const QString &text)
{
    bool ok = false;
    const int retry = text.toInt(&ok);
    return ok && TaskSettingRules::retry.accepts(retry);
}

inline QString groupForItem(const QString &item) { return TestSequenceTaskKind::settingsGroup(item); }

// Run after JSON syntax validation, before Qt's object conversion hides duplicate keys.
inline bool uniqueKeys(const QString &json)
{
    QList<QSet<QString>> objects;
    for (qsizetype i = 0; i < json.size(); ++i) {
        if (json[i] == '{')
            objects.append(QSet<QString>{});
        else if (json[i] == '}')
            objects.removeLast();
        else if (json[i] == '"') {
            const auto start = i++;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\')
                    ++i;
                ++i;
            }
            auto next = i + 1;
            while (next < json.size() && json[next].isSpace())
                ++next;
            if (next < json.size() && json[next] == ':') {
                const auto token = json.mid(start, i - start + 1);
                const auto key = QJsonDocument::fromJson(("[" + token + "]").toUtf8()).array()[0].toString();
                if (objects.last().contains(key))
                    return false;
                objects.last().insert(key);
            }
        }
    }
    return true;
}

inline bool number(const QJsonValue &value, double low, double high, bool integer = false)
{
    const double n = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    return value.isDouble() && std::isfinite(n) && n >= low && n <= high && (!integer || std::floor(n) == n);
}

inline bool choice(const QJsonValue &value, const QStringList &choices)
{
    return value.isString() && choices.contains(value.toString().trimmed().toUpper());
}

inline bool transientSetting(const QString &key, const QJsonValue &v, bool hasDischarge)
{
    if (key == "measure_target")
        return choice(v, {"BOTH", "MAX", "MIN"});
    else if (key == "trigger_edge")
        return choice(v, {"AUTO"});
    else if (key == "trig_timeout_ms")
        return (v.isDouble() && TaskSettingRules::triggerTimeout.accepts(v.toDouble()));
    else if (key == "phase_timeout_ms")
        return (v.isDouble() && TaskSettingRules::phaseTimeout.accepts(v.toDouble()));
    else if (key == "discharge_ms" && hasDischarge)
        return (v.isDouble() && TaskSettingRules::discharge.accepts(v.toDouble()));
    else if (key == "trials_per_level")
        return (v.isDouble() && TaskSettingRules::trials.accepts(v.toDouble()));
    return false;
}

inline bool oscilloscopeSetting(const QString &key, const QJsonValue &value)
{
    return OscilloscopeSettingRules::decode(key, value).has_value();
}

inline bool config(const QString &group, const QJsonObject &cfg, QString &error)
{
    using Kind = TestSequenceTaskKind::Kind;
    const auto kind = TestSequenceTaskKind::fromSettingsGroup(group);
    const auto single = TestSequenceTaskKind::singleKind(kind);
    const bool grouped = TestSequenceTaskKind::isGroup(kind);
    const bool transient = TestSequenceTaskKind::isTransient(kind);
    const bool steady = TestSequenceTaskKind::isSteady(kind);
    const QString load = TestSequenceTaskKind::loadKey(kind);
    QStringList conditions;
    if (steady || transient || kind == Kind::BrownOut) {
        conditions << "ac_input" << "dc_input";
        if (!grouped)
            conditions << load;
    }
    if (single == Kind::Relay || single == Kind::OnShort || single == Kind::ShortOn || single == Kind::OnShortDescent)
        conditions << "relay";
    if (single == Kind::TurnOn || single == Kind::OnShort || single == Kind::ShortOn || single == Kind::OnShortDescent)
        conditions << "discharge";
    for (auto it = cfg.begin(); it != cfg.end(); ++it) {
        const auto key = it.key();
        const auto v = it.value();
        bool valid = false;
        if (group == "capture") {
            valid = (key == "directory" && v.isString()) || (key == "channel" && (v.isDouble() && TaskSettingRules::captureChannel.accepts(v.toDouble())));
            if (key == "formats" && v.isArray()) {
                valid = true;
                QSet<QString> seen;
                for (const auto &format : v.toArray()) {
                    const QString name = format.toString();
                    valid = valid && format.isString() &&
                            QStringList{"PNG", "CSV", "AllCSV", "WFM", "AllWFM"}.contains(name) &&
                            !seen.contains(name);
                    seen.insert(name);
                }
            }
        } else if (grouped && key == load + "_indices") {
            QVector<int> indices;
            valid = v.isArray() && GroupConditionSelection::decode(v.toVariant(), indices);
        } else if (grouped && key == load + "_labels") {
            valid = v.isArray();
            for (const auto& label : v.toArray())
                valid = valid && label.isString();
        } else if (key == "vertical_margin" && (steady || (transient && !TestSequenceTaskKind::isDescent(kind)))) {
            valid = v.isBool();
        } else if (key == TaskSettingRules::triggerChannel.key && (steady || (transient && !TestSequenceTaskKind::isDescent(kind)))) {
            valid = v.isDouble() && TaskSettingRules::triggerChannel.accepts(v.toDouble());
        } else if (key == "capture_enabled" && (transient || steady || kind == Kind::BrownOut)) {
            valid = v.isBool();
        } else if (key == "capture_settings" && (transient || steady || kind == Kind::BrownOut)) {
            valid = v.isObject() && config("capture", v.toObject(), error);
        } else if (key == "condition_refs" && !conditions.isEmpty() && v.isObject()) {
            valid = true;
            const auto refs = v.toObject();
            for (auto ref = refs.begin(); ref != refs.end(); ++ref) {
                if (grouped && ref.key() == load) {
                    valid = valid && ref.value().isArray();
                    QSet<QString> seen;
                    for (const auto& entry : ref.value().toArray()) {
                        valid = valid && entry.isString() && !entry.toString().isEmpty() && !seen.contains(entry.toString());
                        seen.insert(entry.toString());
                    }
                } else {
                    valid = valid && conditions.contains(ref.key()) && ref.value().isString();
                }
            }
        } else if (key.endsWith("_index") && conditions.contains(key.chopped(6))) {
            valid = number(v, -1, std::numeric_limits<int>::max(), true);
        } else if (key.endsWith("_label") && conditions.contains(key.chopped(6))) {
            valid = v.isString();
        } else if ((key == "relay_values" || key == "discharge_values") &&
                   conditions.contains(key.chopped(7))) {
            valid = v.isArray();
            for (const auto &entry : v.toArray())
                valid = valid && entry.isString();
        } else if (key == "delay_ms" && group != "osc" && group != "relay") {
            valid = (v.isDouble() && TaskSettingRules::settle.accepts(v.toDouble()));
        } else if (key == TaskSettingRules::powerOffWait.key && steady) {
            valid = v.isDouble() && TaskSettingRules::powerOffWait.accepts(v.toDouble());
        } else if (key == TaskSettingRules::startupWait.key && steady) {
            valid = v.isDouble() && TaskSettingRules::startupWait.accepts(v.toDouble());
        } else if (key == "auto_period" && steady) {
            valid = v.isBool();
        } else if (key == "measure_target" && steady) {
            valid = choice(v, {"BOTH", "MAX", "MIN"});
        } else if (kind == Kind::TurnOffDescent && key == "trigger_channel_enabled") {
            valid = v.isBool();
        } else if (TestSequenceTaskKind::isDescent(kind) && key == "trigger_channel") {
            valid = DescentSettings::validChannel(v.toVariant());
        } else if (TestSequenceTaskKind::isDescent(kind) && key == "trigger_levels") {
            QVector<double> levels;
            valid = v.isArray() && DescentSettings::decodeLevels(v.toVariant(), levels, true);
        } else if (TestSequenceTaskKind::isDescent(kind) && (key == "measure_target" || key == "trigger_edge")) {
            valid = false; // Descent has fixed Falling edges and explicit levels, not a search direction.
        } else if (transient) {
            valid = transientSetting(key, v, conditions.contains("discharge"));
        } else if (kind == Kind::BrownOut) {
            valid = BrownOut::validField(key, v.toVariant());
        } else if (group == "osc") {
            valid = oscilloscopeSetting(key, v);
        }
        if (!valid) {
            error = "Invalid TestSequence setting: " + group + "." + key;
            return false;
        }
    }
    if (kind == Kind::BrownOut) {
        try { const BrownOut::Settings checked(cfg.toVariantMap()); }
        catch (const std::exception &e) { error = QString::fromUtf8(e.what()); return false; }
    }
    if (grouped) {
        const auto count = cfg.value(load + "_indices").toArray().size();
        const auto refs = cfg.value("condition_refs").toObject();
        if ((cfg.contains(load + "_labels") && cfg.value(load + "_labels").toArray().size() != count) ||
            (refs.contains(load) && refs.value(load).toArray().size() != count)) {
            error = "Group condition indices, labels and references must have matching lengths";
            return false;
        }
    }
    return true;
}

inline bool settings(const QString &item, const QJsonObject &groups, QString &error)
{
    const auto expected = groupForItem(item);
    if (expected.isEmpty()) {
        error = "Unknown TestSequence test item: " + item;
        return false;
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        if (it.key() != expected || !it.value().isObject()) {
            error = "TestSequence settings group does not match " + item + ": " + it.key();
            return false;
        }
        if (!config(it.key(), it.value().toObject(), error))
            return false;
    }
    return true; // Missing settings are an editable draft; execution validates required conditions.
}
} // namespace TestSequenceSettingsValidation
