#include "scopesessioncoordinator.h"
#include "capturelease.h"
#include "oscilloscope.h"
#include "pendingscopes.h"
#include <QFutureWatcher>
#include <QPointer>
#include <QPromise>
#include <QtConcurrent/QtConcurrentRun>

namespace
{
bool hasScope(const Page1Config &config)
{
    for (const auto &instrument : config.instruments)
        if (instrument.type == "Oscilloscope" && instrument.enabled &&
            !instrument.modelName.trimmed().isEmpty())
            return true;
    return false;
}
struct LeasedScope
{
    std::shared_ptr<Oscilloscope> scope;
    std::shared_ptr<CaptureLease> lease;
};
} // namespace
ScopeSessionCoordinator::ScopeSessionCoordinator(Connect connectScopes, QObject *parent)
    : QObject(parent), m_connect(std::move(connectScopes))
{
    QPromise<void> retired;
    retired.start();
    m_retirement = retired.future();
    retired.finish();
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &ScopeSessionCoordinator::applyPending);
    m_leaseObserver.setInterval(50);
    connect(&m_leaseObserver, &QTimer::timeout, this,
            [this]
            {
                const bool busy = captureBusy() || testBusy();
                if (!busy)
                    m_leaseObserver.stop();
                if (busy != m_observedBusy)
                {
                    m_observedBusy = busy;
                    emit stateChanged();
                }
            });
}
ScopeSessionCoordinator::~ScopeSessionCoordinator() { retireScopes(); }
QFuture<void> ScopeSessionCoordinator::retireScopes()
{
    auto completed = std::make_shared<QPromise<void>>();
    completed->start();
    auto preceding = m_retirement;
    m_retirement = completed->future();
    auto scopes = m_scopes.takeAll();
    m_poll->closeWhenIdle(
        [capture = m_capture, test = m_test, scopes = std::move(scopes), preceding, completed]() mutable
        {
            capture->closeWhenIdle(
                [test, scopes = std::move(scopes), preceding, completed]() mutable
                {
                    test->closeWhenIdle(
                        [scopes = std::move(scopes), preceding, completed]() mutable
                        {
                            preceding.then(QtFuture::Launch::Async,
                                           [scopes = std::move(scopes), completed]() mutable
                                           {
                                               OscilloscopeManager::disconnectAll(scopes);
                                               completed->finish();
                                           });
                        });
                });
        });
    m_poll = std::make_shared<CaptureSession>();
    m_capture = std::make_shared<CaptureSession>();
    m_test = std::make_shared<CaptureSession>();
    return m_retirement;
}
void ScopeSessionCoordinator::setBlocked(bool blocked)
{
    if (m_blocked == blocked)
        return;
    m_blocked = blocked;
    if (!blocked && m_updates.hasPending())
        m_debounce.start(500);
}
void ScopeSessionCoordinator::queueConfiguration(const Page1Config &config)
{
    ++m_revision;
    m_updates.enqueue(config);
    m_debounce.start(500);
    if (hasScope(m_config) && !hasScope(config) && !m_blocked && !testBusy() && !captureBusy())
    {
        QPointer<ScopeSessionCoordinator> alive(this);
        emit configurationAccepted(config);
        if (!alive)
            return;
        emit scopesAboutToReset();
        if (alive)
            retireScopes();
    }
}
void ScopeSessionCoordinator::applyPending()
{
    auto snapshot = m_updates.tryStart(m_blocked || captureBusy() || testBusy() || pollingBusy());
    if (!snapshot)
    {
        if (m_updates.hasPending() && !m_updates.isRunning())
            m_debounce.start(1000);
        return;
    }
    m_config = *snapshot;
    const auto revision = m_revision;
    QPointer<ScopeSessionCoordinator> alive(this);
    emit stateChanged();
    if (!alive)
        return;
    emit configurationAccepted(m_config);
    if (!alive)
        return;
    emit scopesAboutToReset();
    if (!alive)
        return;
    auto retired = retireScopes();
    auto *watcher = new QFutureWatcher<std::shared_ptr<PendingScopes>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<PendingScopes>>::finished, this,
            [this, watcher, revision]
            {
                QString error;
                QPointer<ScopeSessionCoordinator> alive(this);
                try
                {
                    auto pending = watcher->result();
                    if (revision == m_revision)
                    {
                        m_scopes.assign(pending->take());
                        emit scopesChanged();
                    }
                    else
                    {
                        // Serialize stale connection cleanup before the next
                        // configuration opens.
                        auto unused = pending->take();
                        m_retirement =
                            m_retirement.then(QtFuture::Launch::Async, [unused = std::move(unused)]() mutable
                                              { OscilloscopeManager::disconnectAll(unused); });
                    }
                }
                catch (const std::exception &e)
                {
                    error = QString::fromUtf8(e.what());
                }
                catch (...)
                {
                    error = tr("Unknown oscilloscope connection error.");
                }
                if (!alive)
                    return;
                watcher->deleteLater();
                m_updates.complete();
                emit stateChanged();
                if (!alive)
                    return;
                if (m_updates.hasPending())
                    m_debounce.start(500);
                if (revision == m_revision && !error.isEmpty())
                    emit connectionFailed(error);
            });
    auto connected =
        retired.then(QtFuture::Launch::Async,
                     [config = std::move(*snapshot), connectScopes = m_connect]
                     {
                         if (!connectScopes)
                             throw std::runtime_error("Scope connection operation is unavailable");
                         return std::make_shared<PendingScopes>(connectScopes(config));
                     });
    m_retirement = connected.then([](const std::shared_ptr<PendingScopes> &) {}).onFailed([] {});
    watcher->setFuture(connected);
}
std::shared_ptr<Oscilloscope> ScopeSessionCoordinator::select(const QString &model)
{
    auto scope = m_scopes.get(model);
    if (scope)
        m_scopes.setCurrent(model);
    return scope;
}
std::shared_ptr<Oscilloscope> ScopeSessionCoordinator::borrowForTest()
{
    if (m_blocked || isConfigurationBusy() || hasPendingConfiguration() || captureBusy() || pollingBusy())
        return {};
    auto scope = current();
    if (!scope || !scope->isConnected())
        return {};
    auto lease = CaptureLease::tryAcquire(m_test);
    if (!lease)
        return {};
    auto owner = std::make_shared<LeasedScope>(LeasedScope{scope, std::move(lease)});
    observeLeases();
    return std::shared_ptr<Oscilloscope>(owner, scope.get());
}
std::function<std::shared_ptr<void>()> ScopeSessionCoordinator::pollingLeaseFactory() const
{
    return [scope = current(), poll = m_poll, capture = m_capture, test = m_test]() -> std::shared_ptr<void>
    {
        if (!scope || capture->isBusy() || test->isBusy())
            return {};
        auto lease = CaptureLease::tryAcquire(poll);
        return lease ? std::make_shared<LeasedScope>(LeasedScope{scope, std::move(lease)}) : nullptr;
    };
}
void ScopeSessionCoordinator::observeLeases()
{
    m_observedBusy = captureBusy() || testBusy();
    if (m_observedBusy)
        m_leaseObserver.start();
}
