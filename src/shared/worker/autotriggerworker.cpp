#include "autotriggerworker.h"
#include "dpo7000.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <cmath>

AutoTriggerWorker::AutoTriggerWorker(DPO7000* instrument, QObject* parent)
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

void AutoTriggerWorker::setTriggerTimeout(int timeoutMs)
{
    QMutexLocker locker(&m_paramsMutex);
    m_triggerTimeout = qBound(500, timeoutMs, 10000);
    qDebug() << QString("[AutoTriggerWorker] Trigger timeout set to: %1ms")
                    .arg(m_triggerTimeout);
}

void AutoTriggerWorker::setMaxFailCount(int count)
{
    QMutexLocker locker(&m_paramsMutex);
    m_maxFailCount = qMax(1, count);
    qDebug() << QString("[AutoTriggerWorker] Max fail count set to: %1")
                    .arg(m_maxFailCount);
}

void AutoTriggerWorker::setTriggerCheckInterval(int intervalMs)
{
    QMutexLocker locker(&m_paramsMutex);
    m_triggerCheckInterval = qBound(50, intervalMs, 500);
    qDebug() << QString("[AutoTriggerWorker] Check interval set to: %1ms")
                    .arg(m_triggerCheckInterval);
}

// ===== 追蹤控制 =====
void AutoTriggerWorker::startTracking()
{
    if (!m_instrument || !m_instrument->isConnected()) {
        emit trackingError("Instrument not connected");
        return;
    }

    QMutexLocker locker(&m_paramsMutex);
    m_isTracking = true;
    m_currentLevel = m_startLevel;
    m_lastSuccessLevel = m_startLevel;
    m_currentFailCount = 0;
    locker.unlock();

    qDebug() << "[AutoTriggerWorker] ========================================";
    qDebug() << "[AutoTriggerWorker] Starting semi-auto trigger tracking";
    qDebug() << "[AutoTriggerWorker] Configuration:";
    qDebug() << QString("    Start Level:     %1 V").arg(m_currentLevel, 0, 'f', 3);
    qDebug() << QString("    Target Level:    %1 V").arg(m_targetLevel, 0, 'f', 3);
    qDebug() << QString("    Step Scale:      %1 V").arg(m_stepScale, 0, 'f', 3);
    qDebug() << QString("    Timeout:         %1 ms").arg(m_triggerTimeout);
    qDebug() << QString("    Max Fail Count:  %1").arg(m_maxFailCount);
    qDebug() << QString("    Check Interval:  %1 ms").arg(m_triggerCheckInterval);
    qDebug() << "[AutoTriggerWorker] ========================================";

    resetTracking();

    // 設定起始觸發電平
    if (!setTriggerLevel(m_currentLevel)) {
        emit trackingError("Failed to set initial trigger level");
        return;
    }

    // 檢查是否已經達到目標
    if (isTargetReached()) {
        emit targetReached(m_currentLevel);
        emit trackingCompleted(true, "Already at target level");
        return;
    }

    // 開始定期調整
    m_timer->start(800);
}

void AutoTriggerWorker::stopTracking()
{
    QMutexLocker locker(&m_paramsMutex);
    m_isTracking = false;
    locker.unlock();

    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }

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

    double targetLevel = m_targetLevel;
    double currentLevel = m_currentLevel;
    int stepCount = m_stepCount;
    locker.unlock();

    // 檢查儀器連線
    if (!m_instrument || !m_instrument->isConnected()) {
        emit trackingError("Instrument disconnected during adjustment");
        return;
    }

    try {
        qDebug() << "";
        qDebug() << QString("[AutoTriggerWorker] ========== Step %1 ==========").arg(stepCount + 1);
        qDebug() << QString("  Current Level:  %1 V").arg(currentLevel, 8, 'f', 3);
        qDebug() << QString("  Target Level:   %1 V").arg(targetLevel, 8, 'f', 3);
        qDebug() << QString("  Last Success:   %1 V").arg(m_lastSuccessLevel, 8, 'f', 3);

        // 計算下一步電平
        double nextLevel = calculateNextStep();
        qDebug() << QString("  Next Level:     %1 V").arg(nextLevel, 8, 'f', 3);

        // 設定新觸發電平
        if (!setTriggerLevel(nextLevel)) {
            emit trackingError(QString("Failed to set trigger level to %1V")
                                   .arg(nextLevel, 0, 'f', 3));
            return;
        }

        // 等待儀器穩定
        qDebug() << "  Waiting for instrument to stabilize (300ms)...";
        QThread::msleep(300);

        // ===== 關鍵：檢查是否成功觸發 =====
        qDebug() << "  Checking trigger success...";
        bool triggerSuccess = checkTriggerSuccess();

        if (triggerSuccess) {
            // ========== 觸發成功 ==========
            qDebug() << QString("SUCCESS at %1V").arg(nextLevel, 0, 'f', 3);

            QMutexLocker locker2(&m_paramsMutex);
            m_currentLevel = nextLevel;
            m_lastSuccessLevel = nextLevel;
            m_currentFailCount = 0;
            m_stepCount++;
            int newStepCount = m_stepCount;
            locker2.unlock();

            emit adjustmentProgress(nextLevel, newStepCount);

            // 檢查是否達到用戶設定的目標
            if (isTargetReached()) {
                qDebug() << QString("TARGET REACHED at %1V in %2 steps")
                                .arg(nextLevel, 0, 'f', 3)
                                .arg(newStepCount);
                m_timer->stop();
                emit targetReached(nextLevel);
                emit trackingCompleted(true,
                                       QString("✓ Successfully reached target level %1V in %2 steps")
                                           .arg(nextLevel, 0, 'f', 3).arg(newStepCount));
            }

        } else {
            // ========== 觸發失敗 ==========
            qWarning() << QString("FAILED at %1V").arg(nextLevel, 0, 'f', 3);
            handleTriggerFailure(nextLevel);
        }

    } catch (const std::exception& e) {
        emit trackingError(QString("Adjustment failed: %1").arg(e.what()));
        qCritical() << "[AutoTriggerWorker] Exception during adjustment:" << e.what();
    } catch (...) {
        emit trackingError("Unknown error occurred during adjustment");
        qCritical() << "[AutoTriggerWorker] Unknown exception during adjustment";
    }
}

// ===== 核心觸發檢測方法 =====
bool AutoTriggerWorker::checkTriggerSuccess()
{
    if (!m_instrument) {
        qWarning() << "    [checkTriggerSuccess] Instrument is null";
        return false;
    }

    try {
        qDebug() << "    [checkTriggerSuccess] Starting trigger verification...";
        qDebug() << QString("      Config: Timeout=%1ms, Interval=%2ms")
                        .arg(m_triggerTimeout)
                        .arg(m_triggerCheckInterval);

        // ===== 策略 1: 啟動單次觸發 =====
        qDebug() << "    [Strategy 1] Triggering single acquisition...";
        m_instrument->single();

        // 短暫等待讓觸發設置生效
        QThread::msleep(100);

        // ===== 策略 2: 輪詢觸發狀態 =====
        qDebug() << "    [Strategy 2] Polling trigger state...";
        bool triggerSuccess = waitForTrigger();

        if (triggerSuccess) {
            qDebug() << "    [checkTriggerSuccess] ✓ Trigger confirmed";
            return true;
        }

        // ===== 策略 3: 檢查採集狀態作為備用 =====
        qDebug() << "    [Strategy 3] Checking acquisition state as fallback...";
        QString acqState = m_instrument->getAcquisitionState();
        qDebug() << QString("      Acquisition state: %1").arg(acqState);

        if (acqState.contains("STOP", Qt::CaseInsensitive) ||
            acqState == "0" ||
            acqState.contains("OFF", Qt::CaseInsensitive)) {
            qDebug() << "    [checkTriggerSuccess] ✓ Confirmed via acquisition state";
            return true;
        }

        qDebug() << "    [checkTriggerSuccess] ✗ All strategies failed";
        return false;

    } catch (const std::exception& e) {
        qWarning() << "[AutoTriggerWorker] checkTriggerSuccess exception:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "[AutoTriggerWorker] checkTriggerSuccess unknown error";
        return false;
    }
}

// ===== 等待觸發完成 =====
bool AutoTriggerWorker::waitForTrigger()
{
    QElapsedTimer timer;
    timer.start();

    int checkCount = 0;
    QString lastState;
    QString lastAcqState;

    while (timer.elapsed() < m_triggerTimeout) {
        checkCount++;

        // 查詢觸發狀態
        QString trigState = queryTriggerState();
        QString acqState = m_instrument->getAcquisitionState();

        // 只在狀態變化時輸出日誌
        if (trigState != lastState || acqState != lastAcqState) {
            qDebug() << QString("      [%1ms] Check #%2: Trig=%3, Acq=%4")
            .arg(timer.elapsed(), 5)
                .arg(checkCount, 2)
                .arg(trigState)
                .arg(acqState);
            lastState = trigState;
            lastAcqState = acqState;
        }

        // ===== 成功條件判斷 =====

        // 條件 1: 觸發狀態為 TRIGGER 或 SAVE
        if (trigState.contains("TRIGGER", Qt::CaseInsensitive) ||
            trigState.contains("SAVE", Qt::CaseInsensitive)) {
            qDebug() << QString("      ✓ Triggered via trigger state after %1ms (%2 checks)")
                            .arg(timer.elapsed())
                            .arg(checkCount);
            return true;
        }

        // 條件 2: Single 模式下，採集狀態變為 STOP
        if (acqState.contains("STOP", Qt::CaseInsensitive) ||
            acqState == "0" ||
            acqState.contains("OFF", Qt::CaseInsensitive)) {

            if (checkCount > 2) {
                qDebug() << QString("      ✓ Triggered (acquisition stopped) after %1ms (%2 checks)")
                                .arg(timer.elapsed())
                                .arg(checkCount);
                return true;
            }
        }

        // 檢查錯誤狀態
        if (trigState.contains("ERROR", Qt::CaseInsensitive)) {
            qWarning() << QString("      ✗ Error state detected: %1").arg(trigState);
            return false;
        }

        QThread::msleep(m_triggerCheckInterval);
    }

    // 超時
    qWarning() << QString("      ✗ TIMEOUT after %1ms (%2 checks)")
                      .arg(timer.elapsed())
                      .arg(checkCount);
    qWarning() << QString("      Final states - Trigger: %1, Acquisition: %2")
                      .arg(lastState)
                      .arg(lastAcqState);

    return false;
}

// ===== 查詢觸發狀態（帶重試）=====
QString AutoTriggerWorker::queryTriggerState()
{
    if (!m_instrument) {
        return "ERROR: No Instrument";
    }

    const int maxRetries = 3;
    int retryCount = 0;

    while (retryCount < maxRetries) {
        try {
            QString state = m_instrument->getTriggerState();
            if (!state.isEmpty()) {
                return state;
            }

            qWarning() << QString("        Query returned empty (retry %1/%2)")
                              .arg(retryCount + 1)
                              .arg(maxRetries);

        } catch (const std::exception& e) {
            qWarning() << QString("        Query failed (retry %1/%2): %3")
            .arg(retryCount + 1)
                .arg(maxRetries)
                .arg(e.what());
        } catch (...) {
            qWarning() << QString("        Query failed (retry %1/%2): Unknown error")
            .arg(retryCount + 1)
                .arg(maxRetries);
        }

        retryCount++;
        if (retryCount < maxRetries) {
            QThread::msleep(50);
        }
    }

    return "ERROR: Query Failed";
}

// ===== 觸發失敗處理 =====
void AutoTriggerWorker::handleTriggerFailure(double failedLevel)
{
    m_currentFailCount++;

    qWarning() << QString("  Trigger failure #%1 at %2V")
                      .arg(m_currentFailCount)
                      .arg(failedLevel, 0, 'f', 3);

    emit adjustmentProgress(failedLevel, m_stepCount);

    if (m_currentFailCount >= m_maxFailCount) {
        qWarning() << QString("  Max fail count reached (%1)").arg(m_maxFailCount);
        qWarning() << QString("  Last success: %1V, Failed at: %2V")
                          .arg(m_lastSuccessLevel, 0, 'f', 3)
                          .arg(failedLevel, 0, 'f', 3);

        handleActualMaximumFound();
    } else {
        qDebug() << QString("  Retrying... (%1/%2 failures)")
        .arg(m_currentFailCount)
            .arg(m_maxFailCount);
    }
}

// ===== 找到實際最大值處理 =====
void AutoTriggerWorker::handleActualMaximumFound()
{
    m_timer->stop();

    QMutexLocker locker(&m_paramsMutex);
    double actualMax = m_lastSuccessLevel;
    double targetLevel = m_targetLevel;
    int stepCount = m_stepCount;
    locker.unlock();

    qDebug() << QString("  Rolling back to %1V").arg(actualMax, 0, 'f', 3);
    setTriggerLevel(actualMax);

    emit actualMaximumFound(actualMax);
    emit targetReached(actualMax);

    QString message = QString(
                          "⚠️ Actual Maximum Trigger Level Found\n"
                          "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
                          "  Actual Maximum:  %1 V\n"
                          "  User Target:     %2 V\n"
                          "  Difference:      %3 V\n"
                          "  Total Steps:     %4\n"
                          "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
                          "The waveform peak does not exceed %1V.\n"
                          "Trigger level has been set to the maximum achievable value."
                          ).arg(actualMax, 0, 'f', 3)
                          .arg(targetLevel, 0, 'f', 3)
                          .arg(targetLevel - actualMax, 0, 'f', 3)
                          .arg(stepCount);

    emit trackingCompleted(true, message);

    qDebug() << "[AutoTriggerWorker] ========================================";
    qDebug() << "[AutoTriggerWorker] Actual maximum found and reported";
    qDebug() << "[AutoTriggerWorker] ========================================";
}

// ===== 其他輔助方法 =====
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
    double step = m_stepScale;

    if (qAbs(difference) <= m_stepScale) {
        return m_targetLevel;
    }

    if (difference > 0) {
        return m_currentLevel + step;
    } else {
        return m_currentLevel - step;
    }
}

bool AutoTriggerWorker::isTargetReached() const
{
    const double tolerance = 1.0;
    return qAbs(m_currentLevel - m_targetLevel) <= tolerance;
}

void AutoTriggerWorker::resetTracking()
{
    m_stepCount = 0;
    m_currentFailCount = 0;
}
