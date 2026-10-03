#pragma once
#include "capturesession.h"
#include "oscilloscopemanager.h"
#include "pendingconfigupdate.h"
#include <QObject>
#include <QTimer>
#include <functional>

// GUI-thread owner of scope replacement and exclusive leases. I/O owns
// snapshots off-thread.
class ScopeSessionCoordinator : public QObject
{
    Q_OBJECT
  public:
    using Connect = std::function<OscilloscopeManager::OscMap(const Page1Config &)>;
    explicit ScopeSessionCoordinator(Connect connectScopes, QObject *parent = nullptr);
    ~ScopeSessionCoordinator() override;
    void queueConfiguration(const Page1Config &config);
    void setBlocked(bool blocked);
    bool isConfigurationBusy() const { return m_updates.isRunning(); }
    bool hasPendingConfiguration() const { return m_updates.hasPending(); }
    bool captureBusy() const { return m_capture->isBusy(); }
    bool pollingBusy() const { return m_poll->isBusy(); }
    bool testBusy() const { return m_test->isBusy(); }
    std::shared_ptr<Oscilloscope> current() const { return m_scopes.current(); }
    std::shared_ptr<Oscilloscope> select(const QString &model);
    std::shared_ptr<Oscilloscope> borrowForTest();
    std::function<std::shared_ptr<void>()> pollingLeaseFactory() const;
    std::shared_ptr<CaptureSession> captureSession() const { return m_capture; }
    void observeLeases();
  signals:
    void configurationAccepted(const Page1Config &config);
    void scopesAboutToReset();
    void scopesChanged();
    void stateChanged();
    void connectionFailed(const QString &error);

  private:
    void applyPending();
    QFuture<void> retireScopes();
    QFuture<void> m_retirement;
    Connect m_connect;
    OscilloscopeManager m_scopes;
    PendingConfigUpdate m_updates;
    Page1Config m_config;
    QTimer m_debounce, m_leaseObserver;
    quint64 m_revision = 0;
    bool m_blocked = false, m_observedBusy = false;
    std::shared_ptr<CaptureSession> m_capture = std::make_shared<CaptureSession>();
    std::shared_ptr<CaptureSession> m_poll = std::make_shared<CaptureSession>();
    std::shared_ptr<CaptureSession> m_test = std::make_shared<CaptureSession>();
};
