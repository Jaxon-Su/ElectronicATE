#pragma once
#include "ioscilloscopemeasurestrategy.h"

// ══════════════════════════════════════════════════════
//  NormalTriggerStrategy — Max + Min 雙向棘輪收斂策略
//
//  Phase 1 — Max 收斂（RISE edge）：
//    有觸發 → 讀 maxVal → triggerLevel = maxVal → 再等
//    timeout → 無法超越 → 收斂到真實 Max
//
//  Phase 2 — Min 收斂（FALL edge）：
//    有觸發 → 讀 minVal → triggerLevel = minVal → 再等
//    timeout → 無法低於 → 收斂到真實 Min
//
//  最終讀取所有啟用通道的 Max / Min / RMS / Mean
// ══════════════════════════════════════════════════════
class NormalTriggerStrategy : public IOscilloscopeMeasureStrategy
{
public:
    explicit NormalTriggerStrategy(int timeoutMs = 10000);

    OscMeasureResult execute(Oscilloscope* scope,
                             QAtomicInt&   stopFlag) override;

    QString name() const override;

private:
    int m_timeoutMs;

    // Phase 1: RISE edge 棘輪收斂，回傳是否正常完成（false = 使用者中止）
    bool ratchetMax(Oscilloscope* scope, QAtomicInt& stopFlag);

    // Phase 2: FALL edge 棘輪收斂，回傳是否正常完成
    bool ratchetMin(Oscilloscope* scope, QAtomicInt& stopFlag);
};
