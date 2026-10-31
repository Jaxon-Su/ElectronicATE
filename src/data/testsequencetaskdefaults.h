#pragma once
#include "testsequencetaskkind.h"
#include "tasksettingrules.h"
#include "brownoutsettings.h"
#include "descentsettings.h"
#include <QVariantMap>

inline QVariantMap testSequenceDefaultSettings(const QString &name)
{
    using Kind = TestSequenceTaskKind::Kind;
    const auto kind = TestSequenceTaskKind::fromName(name);
    if (kind == Kind::Delay)
        return {{"delay", QVariantMap{{TaskSettingRules::settle.key, TaskSettingRules::settle.initial}}}};
    if (kind == Kind::Relay)
        return {{"relay", QVariantMap{{"relay_index", -1}, {"relay_label", ""}}}};
    if (!TestSequenceTaskKind::isMeasurement(name))
        return {};
    QVariantMap settings{{"ac_input_index", -1}, {"ac_input_label", ""},
                         {"dc_input_index", -1}, {"dc_input_label", ""}};
    if (kind == Kind::BrownOut) {
        settings.insert(BrownOut::defaults());
        settings["load_index"] = -1;
        settings["load_label"] = "";
        return {{"brownOut", settings}};
    }
    const auto single = TestSequenceTaskKind::singleKind(kind);
    if (!TestSequenceTaskKind::isDescent(kind)) {
        settings["vertical_margin"] = false;
        settings[TaskSettingRules::triggerChannel.key] = TaskSettingRules::triggerChannel.initial;
    }
    if (TestSequenceTaskKind::isSteady(kind)) {
        settings[TaskSettingRules::powerOffWait.key] = TaskSettingRules::powerOffWait.initial;
        settings[TaskSettingRules::startupWait.key] = TaskSettingRules::startupWait.initial;
    }
    const QString load = TestSequenceTaskKind::loadKey(kind);
    if (TestSequenceTaskKind::isGroup(kind)) {
        settings[load + "_indices"] = QVariantList{};
        settings[load + "_labels"] = QStringList{};
    } else {
        settings[load + "_index"] = -1;
        settings[load + "_label"] = "";
    }
    if (single == Kind::TurnOn)
        settings[TaskSettingRules::triggerTimeout.key] = TaskSettingRules::triggerTimeout.initial;
    else
        settings[TaskSettingRules::settle.key] = TaskSettingRules::settle.initial;
    if (single == Kind::OnShort || single == Kind::ShortOn || single == Kind::OnShortDescent) {
        settings["relay_index"] = -1;
        settings["relay_label"] = "";
    }
    if (single == Kind::TurnOn || single == Kind::OnShort || single == Kind::ShortOn || single == Kind::OnShortDescent) {
        settings["discharge_index"] = -1;
        settings["discharge_label"] = "";
    }
    if (TestSequenceTaskKind::isDescent(kind)) settings.insert(DescentSettings::defaults());
    if (kind == Kind::TurnOffDescent) settings["trigger_channel_enabled"] = true;
    return {{TestSequenceTaskKind::settingsGroup(name), settings}};
}
