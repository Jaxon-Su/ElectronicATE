#pragma once

#include "page5taskpayload.h"
#include "page5taskkind.h"
#include "triggermodelcatalog.h"

namespace Page5ScopePolicy {
inline bool requiresScope(const QString& task)
{
    return Page5TaskKind::isMeasurement(task) || task == "Write Oscilloscope" || task == "Capture";
}

inline int firstScopeTask(const QVector<TaskPayload>& tasks)
{
    for (int i = 0; i < tasks.size(); ++i) {
        if (requiresScope(tasks[i].task.name))
            return i;
    }
    return -1;
}

inline QString unavailableReason(const QString& model)
{
    if (TriggerModelCatalog::family(model) == TriggerModelCatalog::Family::Mso456)
        return {};
    if (model.trimmed().isEmpty())
        return QStringLiteral("請先設定並連線 MSO 4/5/6 系列示波器。");
    return QStringLiteral("%1：Page5 示波器功能尚未完成，暫不啟用。目前僅支援 MSO 4/5/6 系列。").arg(model);
}
} // namespace Page5ScopePolicy
