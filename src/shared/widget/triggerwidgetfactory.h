#pragma once
#include <QString>
#include <QWidget>

class TriggerWidgetFactory {
public:
    static QWidget* createTriggerWidget(const QString& modelName, QWidget* parent, QObject** controller);

private:
    static QWidget* createDPO7000TriggerWidget(QWidget* parent, QObject** controller);
    // static QWidget* createDSO5000Widget(QWidget* parent, QObject** controller);
};
