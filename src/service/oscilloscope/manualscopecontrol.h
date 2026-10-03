#pragma once
#include "scopeoperationrunner.h"
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>
#include <functional>
#include <memory>

class IScopeManualControl;

// Owns manual-command admission and polling; the UI only supplies values and renders results.
class ManualScopeControl : public QObject
{
    Q_OBJECT
  public:
    struct Result {
        QVector<double> measurements;
        QString error;
        bool running = false;
        double levelStep = 0;
    };
    using Completion = std::function<void(const Result &)>;
    using LeaseFactory = std::function<std::shared_ptr<void>()>;
    explicit ManualScopeControl(QObject *parent = nullptr);
    void bind(IScopeManualControl *scope);
    void setLeaseFactory(LeaseFactory factory) { m_lease = std::move(factory); }
    void setSuspended(bool suspended);
    bool isSuspended() const { return m_suspended; }
    bool isBusy() const { return m_io.isBusy(); }
    bool hasActiveCommand() const { return m_commandBusy; }
    bool isConnected() const;
    void poll();
    void single();
    void toggleRun();
    void automatic();
    void normal();
    void setLevel(double level);
    void setType(QString type);
    void setSource(QString source);
    void setSlope(QString slope);
    void readStep(int channel, double maximum, Completion complete);
    void measure(int channel, Completion complete);
  signals:
    void commandActivityChanged();
    void reconnectRequested();
    void runningObserved(bool running);
    void connectionChanged();
    void errorOccurred(const QString &error);

  private:
    void execute(std::function<Result(IScopeManualControl *)> work, Completion complete = {});
    ScopeOperationRunner m_io;
    QTimer m_timer;
    IScopeManualControl *m_scope = nullptr;
    LeaseFactory m_lease;
    quint64 m_generation = 0;
    int m_reconnectTicks = 0;
    bool m_suspended = false, m_commandBusy = false, m_pollFault = false;
};
