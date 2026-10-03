#pragma once
#include "page5capturevalidation.h"
#include "page5taskpayload.h"
#include "page5taskkind.h"
#include "tasksettingrules.h"
#include "groupconditionselection.h"
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace Page5Conditions {
inline bool hasValues(const QVector<QString>& values)
{
    for (const auto& value : values)
        if (!value.trimmed().isEmpty())
            return true;
    return false;
}
inline QString rowOption(const QString& label, const QVector<QString>& values)
{
    if (!hasValues(values))
        return {};
    return label.trimmed().isEmpty() ? QString("Unnamed") : label;
}
inline QString inputTitle(const InputRow& row)
{
    return QString("%1/%2/%3/%4")
        .arg(row.phaseMode.isEmpty() ? "1phase" : row.phaseMode, row.vin, row.frequency, row.phase);
}
inline QStringList inputOptions(const Page5ExecutionContext& ctx)
{
    QStringList options;
    for (const auto& row : ctx.inputs)
        options << (row.vin.trimmed().isEmpty() && row.frequency.trimmed().isEmpty() ? QString()
                                                                                     : inputTitle(row));
    for (int i = 0; i < ctx.dcInputs.size(); ++i) {
        bool configured = false;
        for (const auto& row : ctx.dcInputs[i])
            configured |= !row.vin.trimmed().isEmpty() || !row.currentLimit.trimmed().isEmpty();
        options << (configured
                        ? QString("DC: %1").arg(ctx.dcLabels.value(i).trimmed().isEmpty() ? QString("Unnamed")
                                                                                          : ctx.dcLabels[i])
                        : QString());
    }
    return options;
}
inline QString signature(const QString& label, const QVector<QString>& values)
{
    QJsonArray a;
    a.append(label);
    for (const auto& value : values)
        a.append(value);
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}
inline QStringList signatures(const Page5ExecutionContext& ctx, const QString& key)
{
    QStringList result;
    if (key == "input") {
        for (const auto& row : ctx.inputs)
            result << signature(inputTitle(row), {});
        for (int i = 0; i < ctx.dcInputs.size(); ++i) {
            QVector<QString> values;
            for (const auto& row : ctx.dcInputs[i])
                values << row.vin << row.currentLimit;
            result << signature("DC:" + ctx.dcLabels.value(i), values);
        }
    } else if (key == "load") {
        for (const auto& row : ctx.loads)
            result << (hasValues(row.values) ? signature(row.label, row.values) : QString());
    } else if (key == "dyload") {
        for (int i = 0; i < ctx.dynamics.size(); ++i) {
            auto values = ctx.dynamics[i].values;
            values.append(ctx.dynamicMeta.t1t2.value(i));
            result << (hasValues(ctx.dynamics[i].values) ? signature(ctx.dynamics[i].label, values)
                                                         : QString());
        }
    } else {
        for (const auto& row : ctx.relays)
            result << signature(row.label, row.values);
    }
    if (key == "input") {
        const auto options = inputOptions(ctx);
        for (int i = 0; i < result.size(); ++i)
            if (options.value(i).isEmpty())
                result[i].clear();
    }
    return result;
}
inline QStringList keys()
{
    return {"input", "load", "dyload", "relay", "discharge"};
}

// Called only after the user accepts a condition dialog, never on generic UI refresh.
inline void capture(QVariantMap& cfg, const Page5ExecutionContext& ctx)
{
    QVariantMap refs;
    for (const auto& key : keys()) {
        const int index = cfg.value(key + "_index", -1).toInt();
        const auto list = signatures(ctx, key);
        if (index >= 0 && index < list.size())
            refs[key] = list[index];
    }
    for (const auto &key : {QStringLiteral("load"), QStringLiteral("dyload")}) {
        if (!cfg.contains(key + "_indices"))
            continue;
        QVector<int> indices;
        QVariantList selected;
        const auto list = signatures(ctx, key);
        if (GroupConditionSelection::decode(cfg.value(key + "_indices"), indices))
            for (int index : indices)
                selected.append(list.value(index));
        refs[key] = selected;
    }
    cfg["condition_refs"] = refs;
}
inline bool resolveGroup(QVariantMap& cfg, const Page5ExecutionContext& ctx, const QString& key, QString& error)
{
    if (!cfg.contains(key + "_indices"))
        return true;
    QVector<int> indices;
    const auto refs = cfg.value("condition_refs").toMap().value(key).toList();
    if (!GroupConditionSelection::decode(cfg.value(key + "_indices"), indices) || refs.size() != indices.size()) {
        error = "Group conditions need reselection. Reopen the task settings.";
        return false;
    }
    const auto available = signatures(ctx, key);
    QVector<int> resolved;
    for (const auto& ref : refs) {
        const QString signature = ref.toString();
        const int index = available.indexOf(signature);
        if (ref.metaType().id() != QMetaType::QString || signature.isEmpty() ||
            available.count(signature) != 1 || resolved.contains(index)) {
            error = "A Group condition changed, was removed or is ambiguous. Reselect the group.";
            return false;
        }
        resolved.append(index);
    }
    std::sort(resolved.begin(), resolved.end());
    QVariantList selected, selectedRefs;
    QStringList labels;
    for (int index : resolved) {
        selected.append(index);
        selectedRefs.append(available[index]);
        labels.append(key == "dyload" ? ctx.dynamics[index].label : ctx.loads[index].label);
    }
    cfg[key + "_indices"] = selected;
    cfg[key + "_labels"] = labels;
    auto updatedRefs = cfg.value("condition_refs").toMap();
    updatedRefs[key] = selectedRefs;
    cfg["condition_refs"] = updatedRefs;
    return true;
}
inline bool resolve(QVariantMap& cfg, const Page5ExecutionContext& ctx, QString& error)
{
    for (const auto &key : {QStringLiteral("load"), QStringLiteral("dyload")})
        if (!resolveGroup(cfg, ctx, key, error))
            return false;
    const auto refs = cfg.value("condition_refs").toMap();
    for (const auto& key : keys()) {
        if (!cfg.contains(key + "_index"))
            continue;
        if (cfg.value(key + "_index", -1).toInt() < 0 && !refs.contains(key))
            continue;
        const auto list = signatures(ctx, key);
        const QString ref = refs.value(key).toString();
        if (ref.isEmpty() || list.count(ref) != 1) {
            error = QString("%1 condition changed, was removed, is ambiguous, or needs migration. Reopen the "
                            "task and select it again.")
                        .arg(key);
            return false;
        }
        const int index = list.indexOf(ref);
        cfg[key + "_index"] = index;
        if (key == "input")
            cfg["input_label"] = inputOptions(ctx).value(index);
        else if (key == "load")
            cfg["load_label"] = ctx.loads[index].label;
        else if (key == "dyload")
            cfg["dyload_label"] = ctx.dynamics[index].label;
        else
            cfg[key + "_label"] = ctx.relays[index].label;
    }
    return true;
}
inline void prepareDialog(QVariantMap& cfg, const Page5ExecutionContext& ctx)
{
    QString groupError;
    for (const auto &key : {QStringLiteral("load"), QStringLiteral("dyload")}) {
        if (!resolveGroup(cfg, ctx, key, groupError)) {
            cfg[key + "_indices"] = QVariantList{};
            cfg[key + "_labels"] = QStringList{};
        }
    }
    // Resolve each reference independently; invalid/legacy choices require reselection.
    for (const auto& key : keys()) {
        if (!cfg.contains(key + "_index"))
            continue;
        QVariantMap one{{key + "_index", cfg.value(key + "_index")},
                        {"condition_refs", cfg.value("condition_refs")}};
        QString error;
        if (resolve(one, ctx, error)) {
            cfg[key + "_index"] = one.value(key + "_index");
            if (one.contains(key + "_label"))
                cfg[key + "_label"] = one.value(key + "_label");
        } else {
            cfg[key + "_index"] = -1;
            cfg[key + "_label"] = QString();
        }
    }
}
inline QStringList requiredKeys(const QString& task)
{
    using Kind = Page5TaskKind::Kind;
    const auto kind = Page5TaskKind::fromName(task);
    const auto single = Page5TaskKind::singleKind(kind);
    QStringList required;
    if (Page5TaskKind::isMeasurement(task)) {
        required << "input";
        if (!Page5TaskKind::isGroup(kind))
            required << Page5TaskKind::loadKey(kind);
    }
    if (single == Kind::Relay || single == Kind::OnShort || single == Kind::ShortOn)
        required << "relay";
    if (single == Kind::TurnOn || single == Kind::OnShort || single == Kind::ShortOn)
        required << "discharge";
    return required;
}
inline bool validate(TaskPayload& payload, const Page5ExecutionContext& ctx, QString& error)
{
    if (!TaskSettingRules::retry.accepts(payload.retry)) {
        error = "Fail Retry must be an integer from 0 to 100.";
        return false;
    }
    if (payload.task.name == "Capture" && !validatePage5Capture(payload.settings, error))
        return false;
    if (!resolve(payload.settings, ctx, error))
        return false;
    const auto kind = Page5TaskKind::fromName(payload.task.name);
    if (Page5TaskKind::isGroup(kind)) {
        QVector<int> indices;
        if (!GroupConditionSelection::decode(payload.settings.value(Page5TaskKind::loadKey(kind) + "_indices"), indices) || indices.isEmpty()) {
            error = "Select one or more Load conditions in the Group Test.";
            return false;
        }
    }
    for (const auto& key : requiredKeys(payload.task.name)) {
        if (payload.settings.value(key + "_index", -1).toInt() < 0) {
            error = QString("Select a %1 condition in the task settings.").arg(key);
            return false;
        }
    }
    return true;
}
} // namespace Page5Conditions
