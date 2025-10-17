#include "TriggerControllerFactory.h"
#include "DPO7000TriggerController.h"
#include <QDebug>
// #include "DPO5000TriggerController.h"  // 未來添加

AbstractTriggerController* TriggerControllerFactory::createTriggerController(
    const QString& modelName, QWidget* triggerWidget, QObject* parent)
{
    const QString model = modelName.toUpper().trimmed();

    if (model == "DPO7000") {
        return new DPO7000TriggerController(triggerWidget, parent);
    }
    // 未來可以添加
    else if (model == "DPO5000") {
        // return new DPO5000TriggerController(triggerWidget, parent);
        qWarning() << "[TriggerControllerFactory] DPO5000 controller not implemented yet";
        return nullptr;
    }

    qWarning() << "[TriggerControllerFactory] Unsupported model:" << modelName;
    return nullptr;
}

QStringList TriggerControllerFactory::getSupportedModels()
{
    return {
        "DPO7000",
        "DPO5000"   // 預留
    };
}

bool TriggerControllerFactory::isModelSupported(const QString& modelName)
{
    return getSupportedModels().contains(modelName.toUpper().trimmed());
}
