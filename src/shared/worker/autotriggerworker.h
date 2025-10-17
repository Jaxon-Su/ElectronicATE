#pragma once
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QElapsedTimer>

class DPO7000;

class AutoTriggerWorker : public QObject
{
    Q_OBJECT

public:
    explicit AutoTriggerWorker(DPO7000* instrument, QObject* parent = nullptr);
    ~AutoTriggerWorker();

    // 參數設置方法
    void setStartLevel(double startLevel);
    void setTargetLevel(double target);
    void setStepScale(double scale);
    void setTriggerTimeout(int timeoutMs);
    void setMaxFailCount(int count);
    void setTriggerCheckInterval(int intervalMs);

public slots:
    void startTracking();
    void stopTracking();
    void performAdjustment();

signals:
    void targetReached(double finalLevel);
    void adjustmentProgress(double currentLevel, int stepCount);
    void trackingError(const QString& error);
    void trackingCompleted(bool success, const QString& message);
    void actualMaximumFound(double maxLevel);

private:
    DPO7000* m_instrument = nullptr;
    QTimer* m_timer = nullptr;

    // 執行緒安全的參數
    mutable QMutex m_paramsMutex;
    double m_startLevel = 0.0;
    double m_targetLevel = 0.0;
    double m_stepScale = 0.1;
    double m_currentLevel = 0.0;
    bool m_isTracking = false;
    int m_stepCount = 0;

    // 觸發檢測相關參數
    double m_lastSuccessLevel = 0.0;
    int m_triggerTimeout = 3000;
    int m_triggerCheckInterval = 100;
    int m_maxFailCount = 2;
    int m_currentFailCount = 0;

    // 輔助方法
    bool setTriggerLevel(double level);
    double calculateNextStep();
    bool isTargetReached() const;
    void resetTracking();

    // 觸發檢測方法
    bool checkTriggerSuccess();
    bool waitForTrigger();
    QString queryTriggerState();
    void handleTriggerFailure(double failedLevel);
    void handleActualMaximumFound();
};
