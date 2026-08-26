#pragma once
#include "ioscilloscopemeasurestrategy.h"
#include "page1config.h"
#include "page2config.h"

struct TurnOnRatchetConfig {
    int    timeoutMs    = 10000;
    double maxScaleVDiv = 200.0;
};

// ══════════════════════════════════════════════════════
//  TurnOnRatchetStrategy — 上電暫態棘輪收斂策略
//
//  每次迭代流程：
//    PowerOff → Relay 放電（bulk cap 歸零）→ 設 trigLevel
//    → arm scope（SINGLE）→ PowerOn → waitForTrigger
//
//  Phase 1（RISE）：每次觸發後把 trigLevel 調高到 maxVal
//    → timeout（無法超越）→ 收斂到真實 Max
//
//  Phase 2（FALL）：每次觸發後把 trigLevel 調低到 minVal
//    → timeout → 收斂到真實 Min
//
//  Fallback：Phase 1 與 Phase 2 都從未觸發
//    → autoSetup 確認波形，回傳 errorMessage 但不 fail
// ══════════════════════════════════════════════════════
class TurnOnRatchetStrategy : public IOscilloscopeMeasureStrategy
{
public:
    TurnOnRatchetStrategy(Page1Config           cfg,
                          QString               inputLabel,
                          QVector<RelayDataRow> relayRows,
                          int                   dischargeRelayIdx,
                          TurnOnRatchetConfig   ratchetCfg = {});

    OscMeasureResult execute(Oscilloscope* scope, QAtomicInt& stopFlag) override;
    QString          name()  const override;

private:
    // 棘輪 Phase — 回傳 false 代表使用者中止；hitScaleLimit=true 表示 scale 已達上限
    bool ratchetMax(Oscilloscope* scope, QAtomicInt& stopFlag, double& outMax, bool& hitScaleLimit);
    bool ratchetMin(Oscilloscope* scope, QAtomicInt& stopFlag, double& outMin, bool& hitScaleLimit);

    // 電源 / relay 輔助
    bool powerOff  (QAtomicInt& stopFlag);   // PowerOff + 等待輸出落下
    bool discharge (QAtomicInt& stopFlag);   // RelayOn → 等放電 → RelayOff
    bool powerOn   ();                       // PowerOn

    Page1Config           m_cfg;
    QString               m_inputLabel;
    QVector<RelayDataRow> m_relayRows;
    int                   m_dischargeRelayIdx;
    TurnOnRatchetConfig   m_ratchetCfg;
};
