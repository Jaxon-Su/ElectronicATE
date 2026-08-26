#include "autotriggerworker.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <cmath>

// ★ 建構子改為接受 Oscilloscope*，移除對 dpo7000.h 的直接依賴
AutoTriggerWorker::AutoTriggerWorker(Oscilloscope* instrument, QObject* parent)
    : QObject(parent), m_instrument(instrument)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AutoTriggerWorker::performAdjustment);
}

AutoTriggerWorker::~AutoTriggerWorker()
{
    stopTracking();
}

// ===== 參數設置方法 =====

void AutoTriggerWorker::setStartLevel(double startLevel)
{
    QMutexLocker locker(&m_paramsMutex);
    m_startLevel = startLevel;
    qDebug() << QString("[AutoTriggerWorker] Start level set to: %1V").arg(startLevel, 0, 'f', 3);
}

void AutoTriggerWorker::setTargetLevel(double target)
{
    QMutexLocker locker(&m_paramsMutex);
    m_targetLevel = target;
    qDebug() << QString("[AutoTriggerWorker] Target level set to: %1V").arg(target, 0, 'f', 3);
}

void AutoTriggerWorker::setStepScale(double scale)
{
    QMutexLocker locker(&m_paramsMutex);
    m_stepScale = qAbs(scale);
    qDebug() << QString("[AutoTriggerWorker] Step scale set to: %1V").arg(m_stepScale, 0, 'f', 3);
}

// ===== 追蹤控制 =====

void AutoTriggerWorker::startTracking()
{
    if (!m_instrument || !m_instrument->isConnected()) {
        emit trackingError("Instrument not connected");
        return;
    }

    QMutexLocker locker(&m_paramsMutex);
    m_isTracking   = true;
    m_currentLevel = m_startLevel;
    locker.unlock();

    qDebug() << "[AutoTriggerWorker] ========================================";
    qDebug() << "[AutoTriggerWorker] Starting semi-auto trigger tracking";
    qDebug() << "[AutoTriggerWorker] Configuration:";
    qDebug() << QString("    Start Level:     %1 V").arg(m_startLevel,  0, 'f', 3);
    qDebug() << QString("    Target Level:    %1 V").arg(m_targetLevel, 0, 'f', 3);
    qDebug() << QString("    Step Scale:      %1 V").arg(m_stepScale,   0, 'f', 3);
    qDebug() << "[AutoTriggerWorker] ========================================";

    resetTracking();

    try {
        m_instrument->normal();
        qDebug() << "[AutoTriggerWorker] Set to NORMAL mode for continuous display";

        if (!setTriggerLevel(m_currentLevel)) {
            emit trackingError("Failed to set initial trigger level");
            stopTracking();
            return;
        }

        if (isTargetReached()) {
            emit targetReached(m_currentLevel);
            emit trackingCompleted(true, "Already at target level");
            stopTracking();
            return;
        }

        m_timer->start(800);

    } catch (const std::exception& e) {
        QString errorMsg = QString("Failed to start tracking: %1").arg(e.what());
        qCritical() << "[AutoTriggerWorker]" << errorMsg;
        emit trackingError(errorMsg);
        stopTracking();
    } catch (...) {
        qCritical() << "[AutoTriggerWorker] Unknown error occurred while starting tracking";
        emit trackingError("Unknown error occurred while starting tracking");
        stopTracking();
    }
}

void AutoTriggerWorker::stopTracking()
{
    QMutexLocker locker(&m_paramsMutex);
    m_isTracking = false;
    locker.unlock();

    if (m_timer && m_timer->isActive())
        m_timer->stop();

    qDebug() << "[AutoTriggerWorker] Semi-auto trigger tracking stopped";
}

// ===== 調整邏輯 =====

void AutoTriggerWorker::performAdjustment()
{
    QMutexLocker locker(&m_paramsMutex);
    if (!m_isTracking) {
        qDebug() << "[AutoTriggerWorker] Tracking stopped, skipping adjustment";
        return;
    }
    double targetLevel  = m_targetLevel;
    double currentLevel = m_currentLevel;
    int    stepCount    = m_stepCount;
    locker.unlock();

    if (!m_instrument || !m_instrument->isConnected()) {
        m_timer->stop();
        emit trackingError("Instrument disconnected during adjustment");
        return;
    }

    try {
        qDebug() << "";
        qDebug() << QString("[AutoTriggerWorker] ========== Step %1 ==========").arg(stepCount + 1);
        qDebug() << QString("  Current Level:  %1 V").arg(currentLevel, 8, 'f', 3);
        qDebug() << QString("  Target Level:   %1 V").arg(targetLevel,  8, 'f', 3);

        double nextLevel = calculateNextStep();
        qDebug() << QString("  Next Level:     %1 V").arg(nextLevel, 8, 'f', 3);

        if (!setTriggerLevel(nextLevel)) {
            m_timer->stop();
            emit trackingError(QString("Failed to set trigger level to %1V")
                                   .arg(nextLevel, 0, 'f', 3));
            return;
        }

        qDebug() << "  Waiting for instrument to stabilize (300ms)...";
        QThread::msleep(300);

        QMutexLocker locker2(&m_paramsMutex);
        m_currentLevel = nextLevel;
        m_stepCount++;
        int newStepCount = m_stepCount;
        locker2.unlock();

        qDebug() << QString("  Set to %1V (Step %2)").arg(nextLevel, 0, 'f', 3).arg(newStepCount);
        emit adjustmentProgress(nextLevel, newStepCount);

        if (isTargetReached()) {
            qDebug() << QString("TARGET REACHED at %1V in %2 steps")
            .arg(nextLevel, 0, 'f', 3).arg(newStepCount);
            m_timer->stop();
            emit targetReached(nextLevel);
            emit trackingCompleted(true,
                                   QString("Successfully reached target level %1V in %2 steps")
                                       .arg(nextLevel, 0, 'f', 3).arg(newStepCount));
        }

    } catch (const std::exception& e) {
        m_timer->stop();
        emit trackingError(QString("Adjustment failed: %1").arg(e.what()));
        qCritical() << "[AutoTriggerWorker] Exception during adjustment:" << e.what();
    } catch (...) {
        m_timer->stop();
        emit trackingError("Unknown error occurred during adjustment");
        qCritical() << "[AutoTriggerWorker] Unknown exception during adjustment";
    }
}

// ===== 輔助方法 =====

bool AutoTriggerWorker::setTriggerLevel(double level)
{
    if (!m_instrument) return false;

    try {
        m_instrument->setTriggerLevel(level);
        qDebug() << QString("    Trigger level set to: %1V").arg(level, 0, 'f', 3);
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[AutoTriggerWorker] Failed to set trigger level:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "[AutoTriggerWorker] Unknown error setting trigger level";
        return false;
    }
}

double AutoTriggerWorker::calculateNextStep()
{
    QMutexLocker locker(&m_paramsMutex);
    double difference = m_targetLevel - m_currentLevel;

    if (qAbs(difference) <= m_stepScale)
        return m_targetLevel;

    return (difference > 0)
               ? m_currentLevel + m_stepScale
               : m_currentLevel - m_stepScale;
}

bool AutoTriggerWorker::isTargetReached() const
{
    const double tolerance = 1.0;
    return qAbs(m_currentLevel - m_targetLevel) <= tolerance;
}

void AutoTriggerWorker::resetTracking()
{
    m_stepCount = 0;
    qDebug() << "[AutoTriggerWorker] Tracking state reset";
}
