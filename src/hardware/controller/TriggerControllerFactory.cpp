#include "TriggerControllerFactory.h"
#include "dpo7000triggercontroller.h"
#include "dpo4000triggercontroller.h"
#include "msoseries456triggercontroller.h"
#include "triggermodelcatalog.h"
#include <QDebug>

AbstractTriggerController* TriggerControllerFactory::createTriggerController(
    const QString& modelName, QWidget* triggerWidget, QObject* parent)
{
    switch (TriggerModelCatalog::family(modelName)) {
    case TriggerModelCatalog::Family::Dpo7000:
        return new DPO7000TriggerController(triggerWidget, parent);
    case TriggerModelCatalog::Family::Dpo4000:
        return new DPO4000TriggerController(triggerWidget, parent);
    case TriggerModelCatalog::Family::Mso456:
        return new MSOSeries456TriggerController(triggerWidget, parent);
    case TriggerModelCatalog::Family::Unknown:
        qWarning() << "[TriggerControllerFactory] Unsupported model:" << modelName;
        return nullptr;
    }
    return nullptr;
}

QStringList TriggerControllerFactory::getSupportedModels()
{
    return TriggerModelCatalog::supportedModels();
}

bool TriggerControllerFactory::isModelSupported(const QString& modelName)
{
    return TriggerModelCatalog::isSupported(modelName);
}

QString TriggerControllerFactory::getControllerFamily(const QString& modelName)
{
    return TriggerModelCatalog::familyName(modelName);
}
