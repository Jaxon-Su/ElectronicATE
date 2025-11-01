#pragma once
#include <QString>
#include <QWidget>
#include "AbstractTriggerController.h"

class TriggerControllerFactory
{
public:
    static AbstractTriggerController* createTriggerController(
        const QString& modelName,
        QWidget* triggerWidget,
        QObject* parent = nullptr);

    static QStringList getSupportedModels();
    static bool isModelSupported(const QString& modelName);

private:
    TriggerControllerFactory() = default;
};
