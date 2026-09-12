#pragma once
#include "page5taskpayload.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace Page5Conditions {
inline QString inputTitle(const InputRow& row)
{
    return QString("%1/%2/%3/%4").arg(row.phaseMode.isEmpty() ? "1phase" : row.phaseMode,
                                      row.vin, row.frequency, row.phase);
}
inline QString signature(const QString& label, const QVector<QString>& values)
{
    QJsonArray a;
    a.append(label);
    for (const auto& value : values) a.append(value);
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}
inline QStringList signatures(const Page5ExecutionContext& ctx, const QString& key)
{
    QStringList result;
    if (key == "input") {
        for (const auto& row : ctx.inputs)
            result << signature(inputTitle(row), {});
    } else if (key == "load") {
        for (const auto& row : ctx.loads) result << signature(row.label, row.values);
    } else if (key == "dyload") {
        for (int i = 0; i < ctx.dynamics.size(); ++i) {
            auto values = ctx.dynamics[i].values;
            values.append(ctx.dynamicMeta.t1t2.value(i));
            result << signature(ctx.dynamics[i].label, values);
        }
    } else {
        for (const auto& row : ctx.relays) result << signature(row.label, row.values);
    }
    return result;
}
inline QStringList keys() { return {"input", "load", "dyload", "relay", "discharge"}; }

// Called only after the user accepts a condition dialog, never on generic UI refresh.
inline void capture(QVariantMap& cfg, const Page5ExecutionContext& ctx)
{
    QVariantMap refs;
    for (const auto& key : keys()) {
        const int index = cfg.value(key + "_index", -1).toInt();
        const auto list = signatures(ctx, key);
        if (index >= 0 && index < list.size()) refs[key] = list[index];
    }
    cfg["condition_refs"] = refs;
}
inline bool resolve(QVariantMap& cfg, const Page5ExecutionContext& ctx, QString& error)
{
    const auto refs = cfg.value("condition_refs").toMap();
    for (const auto& key : keys()) {
        if (!cfg.contains(key + "_index")) continue;
        if (cfg.value(key + "_index", -1).toInt() < 0 && !refs.contains(key)) continue;
        const auto list = signatures(ctx, key);
        const QString ref = refs.value(key).toString();
        if (ref.isEmpty() || list.count(ref) != 1) {
            error = QString("%1 condition changed, was removed, is ambiguous, or needs migration. Reopen the task and select it again.").arg(key);
            return false;
        }
        const int index = list.indexOf(ref);
        cfg[key + "_index"] = index;
        if (key == "input") cfg["input_label"] = inputTitle(ctx.inputs[index]);
        else if (key == "load") cfg["load_label"] = ctx.loads[index].label;
        else if (key == "dyload") cfg["dyload_label"] = ctx.dynamics[index].label;
        else cfg[key + "_label"] = ctx.relays[index].label;
    }
    return true;
}
inline void prepareDialog(QVariantMap& cfg, const Page5ExecutionContext& ctx)
{
    // Resolve each reference independently; invalid/legacy choices require reselection.
    for (const auto& key : keys()) {
        if (!cfg.contains(key + "_index")) continue;
        QVariantMap one{{key + "_index", cfg.value(key + "_index")},
                        {"condition_refs", cfg.value("condition_refs")}};
        QString error;
        if (resolve(one, ctx, error)) {
            cfg[key + "_index"] = one.value(key + "_index");
            if (one.contains(key + "_label")) cfg[key + "_label"] = one.value(key + "_label");
        } else {
            cfg[key + "_index"] = -1;
            cfg[key + "_label"] = QString();
        }
    }
}
inline QStringList requiredKeys(const QString& task)
{
    if (task == "Turn on" || task == "Turn off" || task == "Static Test") return {"input", "load"};
    if (task == "Dynamic Test") return {"input", "dyload"};
    if (task == "Turn on then short" || task == "Short then turn on") return {"input", "load", "relay"};
    if (task == "Relay") return {"relay"};
    return {};
}
inline bool validate(TaskPayload& payload, const Page5ExecutionContext& ctx, QString& error)
{
    if (!resolve(payload.settings, ctx, error)) return false;
    for (const auto& key : requiredKeys(payload.task.name)) {
        if (payload.settings.value(key + "_index", -1).toInt() < 0) {
            error = QString("Select a %1 condition in the task settings.").arg(key);
            return false;
        }
    }
    return true;
}
}
