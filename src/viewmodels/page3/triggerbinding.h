#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include "itriggercontroller.h"

// Borrowed controller binding, with one reconnect subscription at a time.
class TriggerBinding : public QObject
{
    Q_OBJECT
  public:
    explicit TriggerBinding(QObject *parent = nullptr) : QObject(parent) {}
    ~TriggerBinding() override;
    bool attach(ITriggerController *controller, const QString &modelName);
    void detach();
    void bindInstrument(Oscilloscope *instrument);
    bool hasController() const { return !m_controller.isNull(); }
    QString modelName() const { return m_modelName; }
    void setPollingLeaseFactory(std::function<std::shared_ptr<void>()> factory)
    {
        if (m_controller)
            m_controller->setPollingLeaseFactory(std::move(factory));
    }
    bool isOperationActive() const { return m_controller && m_controller->isOperationActive(); }
    bool hasActiveCommand() const { return m_controller && m_controller->hasActiveCommand(); }
    void setSuspended(bool suspended)
    {
        if (m_controller)
            m_controller->setSuspended(suspended);
    }

  signals:
    void commandActivityChanged();
    void reconnectRequested();

  private:
    QPointer<ITriggerController> m_controller;
    QString m_modelName;
    QMetaObject::Connection m_reconnect;
    QMetaObject::Connection m_activity;
};
