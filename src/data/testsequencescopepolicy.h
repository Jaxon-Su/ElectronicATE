#pragma once

#include "testsequencetaskpayload.h"
#include "testsequencetaskkind.h"
#include "triggermodelcatalog.h"

namespace TestSequenceScopePolicy {
inline bool requiresScope(const QString& task)
{
    return TestSequenceTaskKind::isMeasurement(task) || task == "Write Oscilloscope";
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
    return QStringLiteral("%1：TestSequence 示波器功能尚未完成，暫不啟用。目前僅支援 MSO 4/5/6 系列。").arg(model);
}
} // namespace TestSequenceScopePolicy
