#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class Oscilloscope;

// ViewModel-facing contract. Implementations may bind widgets, but clients do
// not need a widget type or a concrete instrument controller header.
class ITriggerController : public QObject
{
    Q_OBJECT
  public:
    explicit ITriggerController(QObject *parent = nullptr) : QObject(parent) {}
    ~ITriggerController() override = default;
    virtual void setInstrument(Oscilloscope *instrument) = 0;
    virtual Oscilloscope *getInstrument() const = 0;
    virtual QString getSupportedModel() const = 0;
    virtual int getSelectedChannel() const { return 0; }
    virtual bool isOperationActive() const { return false; }
    virtual bool hasActiveCommand() const { return false; }
    virtual void setSuspended(bool) {}
    void setPollingLeaseFactory(std::function<std::shared_ptr<void>()> factory)
    {
        m_pollingLease = std::move(factory);
    }

  protected:
    std::function<std::shared_ptr<void>()> m_pollingLease;

  signals:
    void commandActivityChanged();
    void reconnectRequested();
};
