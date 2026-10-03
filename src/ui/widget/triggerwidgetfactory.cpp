#include "triggerwidgetfactory.h"
#include "triggermodelcatalog.h"
#include "msoseries456triggerwidget.h"
#include <QDebug>

QWidget* TriggerWidgetFactory::createTriggerWidget(const QString& modelName, QWidget* parent,
                                                   QObject** triggerController, int totalChannels)
{
    switch (TriggerModelCatalog::family(modelName)) {
    case TriggerModelCatalog::Family::Mso456:
        return createMSOSeries456TriggerWidget(parent, triggerController, totalChannels);
    case TriggerModelCatalog::Family::Unknown:
        qWarning() << "[TriggerWidgetFactory] Unsupported model:" << modelName;
        return nullptr;
    }
    return nullptr;
}

QWidget* TriggerWidgetFactory::createMSOSeries456TriggerWidget(QWidget* parent, QObject** controller,
                                                               int totalChannels)
{
    auto* widget = new MSOSeries456TriggerWidget(totalChannels, parent);
    if (controller)
        *controller = widget->getTriggerController();
    return widget;
}
