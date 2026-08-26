#include "turnonratchetstrategy.h"
#include "oscilloscope.h"
#include "instrumentexecutor.h"
#include <QThread>
#include <QDebug>
#include <QRegularExpression>

// ─────────────────────────────────────────────
//  file-local helpers
// ─────────────────────────────────────────────
static int parseTrigCh(const QString& src)
{
    static const QRegularExpression re("CH(\\d+)", QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(src);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

static bool sleepInterruptible(int ms, QAtomicInt& stop)
{
    const int chunk = 100;
    int elapsed = 0;
    while (elapsed < ms) {
        if (stop.loadAcquire()) return false;
        const int wait = qMin(chunk, ms - elapsed);
        QThread::msleep(static_cast<unsigned long>(wait));
        elapsed += wait;
    }
    return true;
}

// ══════════════════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════════════════
TurnOnRatchetStrategy::TurnOnRatchetStrategy(
    Page1Config           cfg,
    QString               inputLabel,
    QVector<RelayDataRow> relayRows,
    int                   dischargeRelayIdx,
    TurnOnRatchetConfig   ratchetCfg)
    : m_cfg              (std::move(cfg))
    , m_inputLabel       (std::move(inputLabel))
    , m_relayRows        (std::move(relayRows))
    , m_dischargeRelayIdx(dischargeRelayIdx)
    , m_ratchetCfg       (ratchetCfg)
{}

QString TurnOnRatchetStrategy::name() const
{
    return "TurnOnRatchet(Max+Min)";
}

// ══════════════════════════════════════════════════════
//  execute
// ══════════════════════════════════════════════════════
OscMeasureResult TurnOnRatchetStrategy::execute(Oscilloscope* scope, QAtomicInt& stopFlag)
{
    OscMeasureResult result;

    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }

    double maxVal      = 0.0;
    double minVal      = 0.0;
    bool   hitScaleMax = false;
    bool   hitScaleMin = false;

    // ── Phase 1: Max 收斂 ─────────────────────────────
    if (!ratchetMax(scope, stopFlag, maxVal, hitScaleMax)) {
        result.errorMessage = "Stopped by user";
        return result;
    }

    // 收斂後讀取各啟用通道的 Max
    // scale 撞上限時波形仍 clipped，跳過量測並記錄 warning
    QMap<int, OscChannelMeasure> channels;
    if (!hitScaleMax) {
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            if (!scope->isChannelEnabled(ch)) continue;
            OscChannelMeasure m;
            m.channel = ch;
            m.maxVal  = scope->measureSignalPeak(ch, "MAXimum");
            channels.insert(ch, m);
        }
    } else {
        result.errorMessage += QString("Max scale limit (%1 V/div) reached; maxVal unreliable. ")
                                   .arg(m_ratchetCfg.maxScaleVDiv);
    }

    // ── Phase 2: Min 收斂 ─────────────────────────────
    if (!ratchetMin(scope, stopFlag, minVal, hitScaleMin)) {
        result.errorMessage += "Stopped by user";
        return result;
    }

    // 同上，scale 撞上限時跳過 Min 量測
    if (!hitScaleMin) {
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            if (!scope->isChannelEnabled(ch)) continue;
            channels[ch].minVal = scope->measureSignalPeak(ch, "MINimum");
        }
    } else {
        result.errorMessage += QString("Min scale limit (%1 V/div) reached; minVal unreliable. ")
                                   .arg(m_ratchetCfg.maxScaleVDiv);
    }

    // ── Fallback：完全無觸發（排除 scale 撞上限情境）────
    if (maxVal == 0.0 && minVal == 0.0 && !hitScaleMax && !hitScaleMin) {
        qWarning() << "[TurnOnRatchetStrategy] No trigger captured; running autoSetup";
        scope->autoSetup();
        sleepInterruptible(2000, stopFlag);

        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            if (!scope->isChannelEnabled(ch)) continue;
            OscChannelMeasure m;
            m.channel = ch;
            m.maxVal  = scope->measureSignalPeak(ch, "MAXimum");
            m.minVal  = scope->measureSignalPeak(ch, "MINimum");
            m.rms     = scope->measureSignalPeak(ch, "RMS");
            m.mean    = scope->measureSignalPeak(ch, "MEAN");
            channels.insert(ch, m);
        }
        result.errorMessage = "Could not trigger; auto-snapshot taken";
    }

    for (const auto& m : std::as_const(channels))
        result.channels.append(m);

    result.success = true;
    return result;
}

// ══════════════════════════════════════════════════════
//  ratchetMax — RISE edge 棘輪，收斂到真實 Max
// ══════════════════════════════════════════════════════
bool TurnOnRatchetStrategy::ratchetMax(Oscilloscope* scope,
                                        QAtomicInt&   stopFlag,
                                        double&       outMax,
                                        bool&         hitScaleLimit)
{
    const int    trigCh       = parseTrigCh(scope->getTriggerSource());
    const double scale        = (trigCh > 0) ? scope->getChannelScale(trigCh)    : 1.0;
    const double offset       = (trigCh > 0) ? scope->getChannelPosition(trigCh) : 0.0;
    // 起始：通道中心往上 0.5 div（level 範圍 = offset ± 5×scale）
    double       currentLevel = offset + scale * 0.5;

    scope->setTriggerType("EDGE");
    scope->setTriggerSlope("RISE");

    while (true) {
        if (stopFlag.loadAcquire()) { scope->stop(); return false; }

        if (!powerOff(stopFlag))   return false;
        if (!discharge(stopFlag))  return false;

        scope->setTriggerLevel(currentLevel);
        scope->single();
        scope->run();   // arm

        if (!powerOn()) {
            qWarning() << "[TurnOnRatchetStrategy] ratchetMax: powerOn failed";
            scope->stop();
            return true;   // 硬體問題，停止迭代但不算使用者中止
        }

        const bool triggered = scope->waitForOperationComplete(m_ratchetCfg.timeoutMs);

        if (!triggered) {
            qDebug() << "[TurnOnRatchetStrategy] Max converged → trigLevel=" << currentLevel;
            scope->stop();
            break;
        }

        if (trigCh > 0) {
            if (scope->isClipping(trigCh)) {
                const double oldScale = scope->getChannelScale(trigCh);
                if (oldScale >= m_ratchetCfg.maxScaleVDiv) {
                    qWarning() << "[TurnOnRatchetStrategy] Max clip CH" << trigCh
                               << "scale already at limit" << oldScale << "V/div, stop ratchet";
                    hitScaleLimit = true;
                    scope->stop();
                    break;
                }
                const double newScale = qMin(oldScale * 2.0, m_ratchetCfg.maxScaleVDiv);
                scope->setChannelScale(trigCh, newScale);
                qDebug() << "[TurnOnRatchetStrategy] Max clip CH" << trigCh
                         << "scale" << oldScale << "→" << newScale;
                // currentLevel 不變，同等 trigger level 但垂直範圍加倍後重試
            } else {
                const double newLevel = scope->measureSignalPeak(trigCh, "MAXimum");
                outMax       = qMax(outMax, newLevel);
                currentLevel = (currentLevel + outMax) / 2.0;
                qDebug() << "[TurnOnRatchetStrategy] Max ratchet CH" << trigCh
                         << "measured=" << newLevel << "nextLevel=" << currentLevel;
            }
        } else {
            break;  // 無法解析 trigger channel，不再迭代
        }
    }

    return true;
}

// ══════════════════════════════════════════════════════
//  ratchetMin — FALL edge 棘輪，收斂到真實 Min
// ══════════════════════════════════════════════════════
bool TurnOnRatchetStrategy::ratchetMin(Oscilloscope* scope,
                                        QAtomicInt&   stopFlag,
                                        double&       outMin,
                                        bool&         hitScaleLimit)
{
    const int    trigCh       = parseTrigCh(scope->getTriggerSource());
    const double scale        = (trigCh > 0) ? scope->getChannelScale(trigCh)    : 1.0;
    const double offset       = (trigCh > 0) ? scope->getChannelPosition(trigCh) : 0.0;
    // 起始：通道中心往下 0.5 div（FALL 方向）
    double       currentLevel = offset - scale * 0.5;

    scope->setTriggerType("EDGE");
    scope->setTriggerSlope("FALL");

    while (true) {
        if (stopFlag.loadAcquire()) { scope->stop(); return false; }

        if (!powerOff(stopFlag))  return false;
        if (!discharge(stopFlag)) return false;

        scope->setTriggerLevel(currentLevel);
        scope->single();
        scope->run();   // arm

        if (!powerOn()) {
            qWarning() << "[TurnOnRatchetStrategy] ratchetMin: powerOn failed";
            scope->stop();
            return true;
        }

        const bool triggered = scope->waitForOperationComplete(m_ratchetCfg.timeoutMs);

        if (!triggered) {
            qDebug() << "[TurnOnRatchetStrategy] Min converged → trigLevel=" << currentLevel;
            scope->stop();
            break;
        }

        if (trigCh > 0) {
            if (scope->isClipping(trigCh)) {
                const double oldScale = scope->getChannelScale(trigCh);
                if (oldScale >= m_ratchetCfg.maxScaleVDiv) {
                    qWarning() << "[TurnOnRatchetStrategy] Min clip CH" << trigCh
                               << "scale already at limit" << oldScale << "V/div, stop ratchet";
                    hitScaleLimit = true;
                    scope->stop();
                    break;
                }
                const double newScale = qMin(oldScale * 2.0, m_ratchetCfg.maxScaleVDiv);
                scope->setChannelScale(trigCh, newScale);
                qDebug() << "[TurnOnRatchetStrategy] Min clip CH" << trigCh
                         << "scale" << oldScale << "→" << newScale;
            } else {
                const double newLevel = scope->measureSignalPeak(trigCh, "MINimum");
                outMin       = qMin(outMin, newLevel);
                currentLevel = (currentLevel + outMin) / 2.0;
                qDebug() << "[TurnOnRatchetStrategy] Min ratchet CH" << trigCh
                         << "measured=" << newLevel << "nextLevel=" << currentLevel;
            }
        } else {
            break;
        }
    }

    return true;
}

// ══════════════════════════════════════════════════════
//  電源 / relay 輔助
// ══════════════════════════════════════════════════════
bool TurnOnRatchetStrategy::powerOff(QAtomicInt& stopFlag)
{
    auto res = InstrumentExecutor::runInput(m_cfg, m_inputLabel, InputAction::PowerOff);
    if (!res.success)
        qWarning() << "[TurnOnRatchetStrategy] powerOff failed:" << res.errorMessage;

    return sleepInterruptible(300, stopFlag);   // 等待輸出電壓落下
}

bool TurnOnRatchetStrategy::discharge(QAtomicInt& stopFlag)
{
    if (m_dischargeRelayIdx < 0)
        return true;  // 未設定放電 relay，跳過

    auto resOn = InstrumentExecutor::runRelay(
        m_cfg, m_relayRows, m_dischargeRelayIdx, RelayAction::RelayOn);
    if (!resOn.success)
        qWarning() << "[TurnOnRatchetStrategy] discharge relay ON failed:" << resOn.errorMessage;

    if (!sleepInterruptible(10000, stopFlag)) return false;   // 等待 bulk cap 放電

    auto resOff = InstrumentExecutor::runRelay(
        m_cfg, m_relayRows, m_dischargeRelayIdx, RelayAction::RelayOff);
    if (!resOff.success)
        qWarning() << "[TurnOnRatchetStrategy] discharge relay OFF failed:" << resOff.errorMessage;

    return true;
}

bool TurnOnRatchetStrategy::powerOn()
{
    auto res = InstrumentExecutor::runInput(m_cfg, m_inputLabel, InputAction::PowerOn);
    if (!res.success)
        qWarning() << "[TurnOnRatchetStrategy] powerOn failed:" << res.errorMessage;
    return res.success;
}
