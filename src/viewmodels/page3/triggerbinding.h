#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include "itriggercontroller.h"

// Borrowed controller binding, with one reconnect subscription at a time.
class TriggerBinding : public QObject {
    Q_OBJECT
public:
    explicit TriggerBinding(QObject* parent = nullptr) : QObject(parent) {}
    ~TriggerBinding() override;
    bool attach(ITriggerController* controller, const QString& modelName);
    void detach();
    void bindInstrument(Oscilloscope* instrument);
    bool hasController() const { return !m_controller.isNull(); }
    QString modelName() const { return m_modelName; }
    int selectedChannel() const;

signals:
    void reconnectRequested();

private:
    QPointer<ITriggerController> m_controller;
    QString m_modelName;
    QMetaObject::Connection m_reconnect;
};
