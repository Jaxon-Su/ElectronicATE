#include "TriggerControllerFactory.h"
#include "dpo7000triggercontroller.h"
#include "dpo4000triggercontroller.h"
#include "msoseries456triggercontroller.h"
#include <QDebug>

AbstractTriggerController* TriggerControllerFactory::createTriggerController(
    const QString& modelName, QWidget* triggerWidget, QObject* parent)
{
    const QString model = modelName.toUpper().trimmed();

    if (model == "DPO7000")
        return new DPO7000TriggerController(triggerWidget, parent);

    if (model == "DPO4000" || model == "MSO4000")
        return new DPO4000TriggerController(triggerWidget, parent);

    // MSO 4/5/6 Series（所有機型共用同一 Controller）
    if (model == "MSO44"   || model == "MSO44B"  ||
        model == "MSO46"   || model == "MSO46B"  ||
        model == "MSO54"   || model == "MSO54B"  ||
        model == "MSO56"   || model == "MSO56B"  ||
        model == "MSO58"   || model == "MSO58B"  ||
        model == "MSO58LP" ||
        model == "MSO64"   || model == "MSO64B"  ||
        model == "MSO66B"  || model == "MSO68B"  ||
        model == "LPD64"   || model == "MSOSERIES456")
        return new MSOSeries456TriggerController(triggerWidget, parent);

    qWarning() << "[TriggerControllerFactory] Unsupported model:" << modelName;
    return nullptr;
}

QStringList TriggerControllerFactory::getSupportedModels()
{
    return {
        "DPO7000",
        "DPO4000", "MSO4000",
        // MSO 4/5/6 Series
        "MSO44", "MSO44B", "MSO46", "MSO46B",
        "MSO54", "MSO54B", "MSO56", "MSO56B",
        "MSO58", "MSO58B", "MSO58LP",
        "MSO64", "MSO64B", "MSO66B", "MSO68B",
        "LPD64", "MSOSeries456"
    };
}

bool TriggerControllerFactory::isModelSupported(const QString& modelName)
{
    return getSupportedModels().contains(modelName.toUpper().trimmed());
}

QString TriggerControllerFactory::getControllerFamily(const QString& modelName)
{
    const QString model = modelName.toUpper().trimmed();

    if (model == "DPO7000")
        return QStringLiteral("DPO7000");

    if (model == "DPO4000" || model == "MSO4000")
        return QStringLiteral("DPO4000");

    if (model == "MSO44"   || model == "MSO44B"  ||
        model == "MSO46"   || model == "MSO46B"  ||
        model == "MSO54"   || model == "MSO54B"  ||
        model == "MSO56"   || model == "MSO56B"  ||
        model == "MSO58"   || model == "MSO58B"  ||
        model == "MSO58LP" ||
        model == "MSO64"   || model == "MSO64B"  ||
        model == "MSO66B"  || model == "MSO68B"  ||
        model == "LPD64"   || model == "MSOSERIES456")
        return QStringLiteral("MSOSeries456");

    return model; // 未知機型回傳原字串（保持向後相容）
}
