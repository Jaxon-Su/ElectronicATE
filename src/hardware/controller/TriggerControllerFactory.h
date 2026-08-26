#pragma once
#include <QString>
#include <QWidget>
#include "abstracttriggercontroller.h"

class TriggerControllerFactory
{
public:
    static AbstractTriggerController* createTriggerController(
        const QString& modelName,
        QWidget* triggerWidget,
        QObject* parent = nullptr);

    static QStringList getSupportedModels();
    static bool isModelSupported(const QString& modelName);

    // 將具體機型名稱對應到 Controller 系列名稱（用於模型匹配判斷）
    // 例："MSO44B" → "MSOSeries456"，"DPO7000" → "DPO7000"
    static QString getControllerFamily(const QString& modelName);

private:
    TriggerControllerFactory() = default;
};
