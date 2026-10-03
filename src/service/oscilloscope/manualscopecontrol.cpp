#include "manualscopecontrol.h"
#include "iscopemanualcontrol.h"
#include <QPointer>
#include <cmath>

ManualScopeControl::ManualScopeControl(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &ManualScopeControl::poll);
    m_timer.start();
}
void ManualScopeControl::bind(IScopeManualControl *scope)
{
    ++m_generation;
    m_pollFault = false;
    m_reconnectTicks = 0;
    m_scope = scope;
    emit connectionChanged();
}
bool ManualScopeControl::isConnected() const
{
    return !m_pollFault && m_scope && m_scope->isConnected();
}
void ManualScopeControl::setSuspended(bool suspended)
{
    m_suspended = suspended;
    if (suspended)
        m_timer.stop();
    else
        m_timer.start();
}
void ManualScopeControl::poll()
{
    if (m_suspended || isBusy())
        return;
    if (!isConnected()) {
        QPointer<ManualScopeControl> alive(this);
        emit connectionChanged();
        if (alive && ++m_reconnectTicks >= 10) {
            m_reconnectTicks = 0;
            emit reconnectRequested();
        }
        return;
    }
    auto lease = m_lease ? m_lease() : nullptr;
    if (!lease)
        return;
    auto *scope = m_scope;
    const auto generation = m_generation;
    m_io.submit<Result>(
        std::move(lease),
        [scope] {
            Result result;
            result.running = scope->isRunning();
            result.error = scope->lastError();
            if (!result.error.isEmpty())
                scope->disconnect();
            return result;
        },
        [this, generation](const Result &result) {
            if (generation != m_generation)
                return;
            m_reconnectTicks = 0;
            if (!result.error.isEmpty()) {
                m_pollFault = true;
                emit connectionChanged();
            } else if (!m_suspended)
                emit runningObserved(result.running);
        });
}
void ManualScopeControl::execute(std::function<Result(IScopeManualControl *)> work, Completion complete)
{
    if (m_suspended || isBusy() || !isConnected())
        return;
    auto lease = m_lease ? m_lease() : nullptr;
    if (!lease)
        return;
    auto *scope = m_scope;
    const auto generation = m_generation;
    m_commandBusy = true;
    m_io.submit<Result>(
        std::move(lease),
        [scope, work = std::move(work)] {
            auto result = work(scope);
            if (result.error.isEmpty())
                result.error = scope->lastError();
            return result;
        },
        [this, generation, complete = std::move(complete)](const Result &result) {
            m_commandBusy = false;
            QPointer<ManualScopeControl> alive(this);
            emit commandActivityChanged();
            if (!alive || generation != m_generation)
                return;
            if (!result.error.isEmpty())
                emit errorOccurred(result.error);
            else if (complete)
                complete(result);
        });
    emit commandActivityChanged();
}
void ManualScopeControl::single()
{
    execute([](IScopeManualControl *s) {
        return s->beginSingleAcquisition() ? Result{} : Result{{}, "Single acquisition failed"};
    });
}
void ManualScopeControl::toggleRun()
{
    execute([](IScopeManualControl *s) {
        const bool running = s->isRunning();
        if (!s->lastError().isEmpty())
            return Result{{}, s->lastError()};
        if (running)
            s->stop();
        else
            s->run();
        return Result{};
    });
}
void ManualScopeControl::automatic()
{
    execute([](IScopeManualControl *s) {
        s->automode();
        return Result{};
    });
}
void ManualScopeControl::normal()
{
    execute([](IScopeManualControl *s) {
        s->normal();
        return Result{};
    });
}
void ManualScopeControl::setLevel(double v)
{
    execute([v](IScopeManualControl *s) {
        s->setTriggerLevel(v);
        return Result{};
    });
}
void ManualScopeControl::setType(QString v)
{
    execute([v](IScopeManualControl *s) {
        s->setTriggerType(v);
        return Result{};
    });
}
void ManualScopeControl::setSource(QString v)
{
    execute([v](IScopeManualControl *s) {
        s->setTriggerSource(v);
        return Result{};
    });
}
void ManualScopeControl::setSlope(QString v)
{
    execute([v](IScopeManualControl *s) {
        s->setTriggerSlope(v);
        return Result{};
    });
}
void ManualScopeControl::readStep(int channel, double maximum, Completion complete)
{
    execute(
        [channel, maximum](IScopeManualControl *s) {
            if (channel < 1 || channel > s->getTotalChannel())
                return Result{{}, tr("Select a valid Trigger Source channel.")};
            const double scale = s->getChannelScale(channel);
            if (!s->lastError().isEmpty())
                return Result{{}, s->lastError()};
            if (!std::isfinite(scale) || scale <= 0)
                return Result{{}, tr("Cannot read a valid scale for CH%1.").arg(channel)};
            Result result;
            result.levelStep = scale / 25.0;
            if (result.levelStep < 1e-12 || result.levelStep > maximum)
                result.error = tr("CH%1 scale / 25 is outside the supported Step range.").arg(channel);
            return result;
        },
        std::move(complete));
}
void ManualScopeControl::measure(int channel, Completion complete)
{
    execute(
        [channel](IScopeManualControl *s) {
            Result result;
            for (const auto type : {ScopeMeasurement::Maximum, ScopeMeasurement::Minimum,
                                    ScopeMeasurement::Rms, ScopeMeasurement::Mean}) {
                const double value = s->readMeasurement(channel, type);
                if (!std::isfinite(value))
                    return Result{{}, "Measurement unavailable"};
                result.measurements.append(value);
            }
            return result;
        },
        std::move(complete));
}
