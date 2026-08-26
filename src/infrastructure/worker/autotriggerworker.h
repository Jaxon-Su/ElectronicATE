#pragma once
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QElapsedTimer>
#include "oscilloscope.h"   // 改用基底類別，不再依賴 DPO7000

class AutoTriggerWorker : public QObject
{
    Q_OBJECT

public:
    // ★ 建構子改為接受 Oscilloscope*，DPO7000 / DPO4000 皆可傳入
    explicit AutoTriggerWorker(Oscilloscope* instrument, QObject* parent = nullptr);
    ~AutoTriggerWorker();

    // 參數設置方法
    void setStartLevel(double startLevel);
    void setTargetLevel(double target);
    void setStepScale(double scale);

public slots:
    void startTracking();
    void stopTracking();
    void performAdjustment();

signals:
    void targetReached(double finalLevel);
    void adjustmentProgress(double currentLevel, int stepCount);
    void trackingError(const QString& error);
    void trackingCompleted(bool success, const QString& message);

private:
    Oscilloscope* m_instrument = nullptr;   // ★ 改為基底類別指標
    QTimer*       m_timer      = nullptr;

    // 執行緒安全的參數
    mutable QMutex m_paramsMutex;
    double m_startLevel   = 0.0;
    double m_targetLevel  = 0.0;
    double m_stepScale    = 0.1;
    double m_currentLevel = 0.0;
    bool   m_isTracking   = false;
    int    m_stepCount    = 0;

    // 輔助方法
    bool   setTriggerLevel(double level);
    double calculateNextStep();
    bool   isTargetReached() const;
    void   resetTracking();
};
