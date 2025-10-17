#include "triggerwidgetfactory.h"
#include "dpo7000triggerwidget.h"

QWidget* TriggerWidgetFactory::createTriggerWidget(const QString& modelName,
                                                   QWidget* parent,
                                                   QObject** triggerController)
{
    if (modelName == "DPO7000") {
        return createDPO7000TriggerWidget(parent, triggerController);
    }
    return nullptr;
}

QWidget* TriggerWidgetFactory::createDPO7000TriggerWidget(QWidget* parent, QObject** controller)
{
    auto* widget = new DPO7000TriggerWidget(parent);

    if (controller) {
        *controller = widget->getTriggerController();
    }

    return widget;
}
