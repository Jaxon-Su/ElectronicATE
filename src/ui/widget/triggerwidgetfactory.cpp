#include "triggerwidgetfactory.h"
#include "dpo7000triggerwidget.h"
#include "dpo4000triggerwidget.h"
#include "msoseries456triggerwidget.h"
#include <QDebug>

QWidget* TriggerWidgetFactory::createTriggerWidget(const QString& modelName,
                                                   QWidget* parent,
                                                   QObject** triggerController,
                                                   int totalChannels)
{
    const QString model = modelName.toUpper().trimmed();

    if (model == "DPO7000")
        return createDPO7000TriggerWidget(parent, triggerController);

    if (model == "DPO4000" || model == "MSO4000")
        return createDPO4000TriggerWidget(parent, triggerController);

    // MSO 4/5/6 Series（所有機型共用同一 Widget，通道數由 totalChannels 決定）
    if (model == "MSO44"   || model == "MSO44B"  ||
        model == "MSO46"   || model == "MSO46B"  ||
        model == "MSO54"   || model == "MSO54B"  ||
        model == "MSO56"   || model == "MSO56B"  ||
        model == "MSO58"   || model == "MSO58B"  ||
        model == "MSO58LP" ||
        model == "MSO64"   || model == "MSO64B"  ||
        model == "MSO66B"  || model == "MSO68B"  ||
        model == "LPD64"   || model == "MSOSERIES456")
        return createMSOSeries456TriggerWidget(parent, triggerController, totalChannels);

    qWarning() << "[TriggerWidgetFactory] Unsupported model:" << modelName;
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
