#include "normaltriggerstrategy.h"
#include "oscilloscope.h"
#include <QDebug>
#include <QRegularExpression>

// 從 getTriggerSource() 回傳的字串（如 "CH2", "CH1"）解析通道號；失敗回傳 -1
static int parseTriggerChannel(const QString& src)
{
    static const QRegularExpression re("CH(\\d+)", QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(src);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

NormalTriggerStrategy::NormalTriggerStrategy(int timeoutMs)
    : m_timeoutMs(timeoutMs)
{}

QString NormalTriggerStrategy::name() const
{
    return "NormalTrigger(Max+Min)";
}

// ══════════════════════════════════════════════════════
//  execute — Max 收斂 → Min 收斂 → 讀取所有通道
// ══════════════════════════════════════════════════════
OscMeasureResult NormalTriggerStrategy::execute(Oscilloscope* scope,
                                                QAtomicInt&   stopFlag)
{
    OscMeasureResult result;

    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }

    // ── Phase 1: Max 收斂 ─────────────────────────────
    if (!ratchetMax(scope, stopFlag)) {
        result.errorMessage = "Stopped by user";
        return result;
    }

    // 收斂後讀取各啟用通道的 Max
    QMap<int, OscChannelMeasure> channels;
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        if (!scope->isChannelEnabled(ch)) continue;
        OscChannelMeasure m;
        m.channel = ch;
        m.maxVal  = scope->measureSignalPeak(ch, "MAXimum");
        channels.insert(ch, m);
    }

    // ── Phase 2: Min 收斂 ─────────────────────────────
    if (!ratchetMin(scope, stopFlag)) {
        result.errorMessage = "Stopped by user";
        return result;
    }

    // 收斂後讀取各啟用通道的 Min / RMS / Mean
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        if (!scope->isChannelEnabled(ch)) continue;
        auto& m  = channels[ch];
        m.minVal = scope->measureSignalPeak(ch, "MINimum");
        // m.rms    = scope->measureSignalPeak(ch, "RMS");
        // m.mean   = scope->measureSignalPeak(ch, "MEAN");
    }

    for (auto& m : channels)
        result.channels.append(m);

    result.success = true;
    return result;
}

// ── Phase 1: RISE edge 棘輪，每次觸發後把 triggerLevel 調高到 maxVal ──
bool NormalTriggerStrategy::ratchetMax(Oscilloscope* scope, QAtomicInt& stopFlag)
{
    const int trigCh = parseTriggerChannel(scope->getTriggerSource());

    scope->setTriggerSlope("RISE");
    scope->normal();
    scope->run();

    while (true) {
        if (stopFlag.loadAcquire()) { scope->stop(); return false; }

        const bool triggered = scope->waitForOperationComplete(m_timeoutMs);
        if (!triggered) {
            qDebug() << "[NormalTriggerStrategy] Max converged → STOP";
            break;
        }

        if (trigCh > 0) {
            double newLevel = scope->measureSignalPeak(trigCh, "MAXimum");
            qDebug() << "[NormalTriggerStrategy] Max ratchet CH" << trigCh << "level ->" << newLevel;
            scope->setTriggerLevel(newLevel);
        }

        scope->run();
    }

    scope->stop();
    return true;
}

// ── Phase 2: FALL edge 棘輪，每次觸發後把 triggerLevel 調低到 minVal ──
bool NormalTriggerStrategy::ratchetMin(Oscilloscope* scope, QAtomicInt& stopFlag)
{
    const int trigCh = parseTriggerChannel(scope->getTriggerSource());

    scope->setTriggerSlope("FALL");
    scope->normal();
    scope->run();

    while (true) {
        if (stopFlag.loadAcquire()) { scope->stop(); return false; }

        const bool triggered = scope->waitForOperationComplete(m_timeoutMs);
        if (!triggered) {
            qDebug() << "[NormalTriggerStrategy] Min converged → STOP";
            break;
        }

        if (trigCh > 0) {
            double newLevel = scope->measureSignalPeak(trigCh, "MINimum");
            qDebug() << "[NormalTriggerStrategy] Min ratchet CH" << trigCh << "level ->" << newLevel;
            scope->setTriggerLevel(newLevel);
        }

        scope->run();
    }

    scope->stop();
    return true;
}
