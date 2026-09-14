#include "triggerwidgetfactory.h"
#include "triggermodelcatalog.h"
#include "dpo7000triggerwidget.h"
#include "dpo4000triggerwidget.h"
#include "msoseries456triggerwidget.h"
#include <QDebug>

QWidget* TriggerWidgetFactory::createTriggerWidget(const QString& modelName,
                                                   QWidget* parent,
                                                   QObject** triggerController,
                                                   int totalChannels)
{
    switch (TriggerModelCatalog::family(modelName)) {
    case TriggerModelCatalog::Family::Dpo7000:
        return createDPO7000TriggerWidget(parent, triggerController);
    case TriggerModelCatalog::Family::Dpo4000:
        return createDPO4000TriggerWidget(parent, triggerController);
    case TriggerModelCatalog::Family::Mso456:
        return createMSOSeries456TriggerWidget(parent, triggerController, totalChannels);
    case TriggerModelCatalog::Family::Unknown:
        qWarning() << "[TriggerWidgetFactory] Unsupported model:" << modelName;
        return nullptr;
    }
    return nullptr;
}

QWidget* TriggerWidgetFactory::createDPO7000TriggerWidget(QWidget* parent, QObject** controller)
{
    auto* widget = new DPO7000TriggerWidget(parent);
    if (controller)
        *controller = widget->getTriggerController();
    return widget;
}

QWidget* TriggerWidgetFactory::createDPO4000TriggerWidget(QWidget* parent, QObject** controller)
{
    auto* widget = new DPO4000TriggerWidget(parent);
    if (controller)
        *controller = widget->getTriggerController();
    return widget;
}

QWidget* TriggerWidgetFactory::createMSOSeries456TriggerWidget(
    QWidget* parent, QObject** controller, int totalChannels)
{
    auto* widget = new MSOSeries456TriggerWidget(totalChannels, parent);
    if (controller)
        *controller = widget->getTriggerController();
    return widget;
}
