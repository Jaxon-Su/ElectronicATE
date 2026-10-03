#pragma once
#include "page5taskkind.h"
#include "tasksettingrules.h"
#include "brownoutsettings.h"
#include <QVariantMap>

inline QVariantMap page5DefaultSettings(const QString &name)
{
    using Kind = Page5TaskKind::Kind;
    const auto kind = Page5TaskKind::fromName(name);
    if (kind == Kind::Delay)
        return {{"delay", QVariantMap{{TaskSettingRules::settle.key, TaskSettingRules::settle.initial}}}};
    if (kind == Kind::Relay)
        return {{"relay", QVariantMap{{"relay_index", -1}, {"relay_label", ""}}}};
    if (!Page5TaskKind::isMeasurement(name))
        return {};
    QVariantMap settings{{"ac_input_index", -1}, {"ac_input_label", ""},
                         {"dc_input_index", -1}, {"dc_input_label", ""}};
    if (kind == Kind::BrownOut) {
        settings.insert(BrownOut::defaults());
        settings["load_index"] = -1;
        settings["load_label"] = "";
        return {{"brownOut", settings}};
    }
    const auto single = Page5TaskKind::singleKind(kind);
    if (Page5TaskKind::isSteady(kind)) {
        settings[TaskSettingRules::powerOffWait.key] = TaskSettingRules::powerOffWait.initial;
        settings[TaskSettingRules::startupWait.key] = TaskSettingRules::startupWait.initial;
    }
    const QString load = Page5TaskKind::loadKey(kind);
    if (Page5TaskKind::isGroup(kind)) {
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
    if (single == Kind::OnShort || single == Kind::ShortOn) {
        settings["relay_index"] = -1;
        settings["relay_label"] = "";
    }
    if (single == Kind::TurnOn || single == Kind::OnShort || single == Kind::ShortOn) {
        settings["discharge_index"] = -1;
        settings["discharge_label"] = "";
    }
    return {{Page5TaskKind::settingsGroup(name), settings}};
}
