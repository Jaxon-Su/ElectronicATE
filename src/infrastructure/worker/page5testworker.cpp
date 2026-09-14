#include <exception>
#include "page5testworker.h"

#include "oscilloscope.h"
#include "instrumentexecutor.h"
#include "ioscilloscopemeasurestrategy.h"
#include "oscilloscopestrategyfactory.h"
#include "turnonratchetstrategy.h"
#include <QThread>
#include <QDebug>
#include <memory>

Page5TestWorker::Page5TestWorker(QObject* parent)
    : QObject(parent)

{}

void Page5TestWorker::stop()
{
    m_stopRequested.storeRelease(1);
}

// ─────────────────────────────────────────────
//  startTasks：主執行迴圈（worker thread）
// ─────────────────────────────────────────────
void Page5TestWorker::startTasks(const QVector<TaskPayload>& payloads, const Page5ExecutionContext& context)
{
    m_context = context;
    emit logMessage("=== Test started ===");

    for (int i = 0; i < payloads.size(); ++i) {
        if (m_stopRequested.loadAcquire()) {
            emit logMessage(QString("[%1] Stopped by user.").arg(i + 1));
            break;
        }

        const TaskPayload& p = payloads[i];
        emit logMessage(QString("[%1] Running: %2  (uid=%3)")
                            .arg(i + 1).arg(p.task.name).arg(p.task.dutUid));
        emit taskStatusChanged(i, TaskStatus::Running);

        bool pass = false;
        for (int attempt = 0; attempt <= p.retry; ++attempt) {
            if (attempt > 0) {
                emit logMessage(QString("[%1] Retry %2/%3").arg(i + 1).arg(attempt).arg(p.retry));
                emit retryCountChanged(i, attempt);
            }
            try {
                pass = executeTask(i, p);
            } catch (const std::exception& ex) {
                emit logMessage(QString("Task exception: %1").arg(ex.what()));
                pass = false;
            } catch (...) {
                emit logMessage("Unknown task exception");
                pass = false;
            }
            if (pass) break;
            if (m_stopRequested.loadAcquire()) break;
        }

        emit taskStatusChanged(i, pass ? TaskStatus::Pass
                                       : TaskStatus::Fail);
        emit logMessage(QString("[%1] %2 → %3")
                            .arg(i + 1).arg(p.task.name).arg(pass ? "Pass" : "Fail"));
    }

    emit logMessage("=== Test finished ===");
    emit finished();
}

// ─────────────────────────────────────────────
//  executeTask：依 name 分派
// ─────────────────────────────────────────────
bool Page5TestWorker::executeTask(int /*idx*/, const TaskPayload& p)
{
    const QString& name = p.task.name;
    const QVariantMap& cfg = p.settings;

    if      (name == "Delay")                return executeDelay(cfg);
    else if (name == "Write Oscilloscope")   return executeWriteOscilloscope(cfg);
    else if (name == "Turn on")              return executeTurnOn(cfg);
    else if (name == "Turn off")             return executeTurnOff(cfg);
    else if (name == "Relay")                return executeRelay(cfg);
    else if (name == "Static Test")          return executeStaticTest(cfg);
    else if (name == "Dynamic Test")         return executeDynamicTest(cfg);
    else if (name == "Turn on then short")   return executeTurnOnThenShort(cfg);
    else if (name == "Short then turn on")   return executeShortThenTurnOn(cfg);

    emit logMessage(QString("  [WARN] Unknown task: %1").arg(name));
    return false;
}

// ══════════════════════════════════════════════════════
//  各 Task 執行（Stub）— 後續填入實際儀器控制邏輯
// ══════════════════════════════════════════════════════

bool Page5TestWorker::executeDelay(const QVariantMap& cfg)
{
    const int    totalMs = cfg.value("delay_ms", 5000).toInt();
    const double seconds = totalMs / 1000.0;
    emit logMessage(QString("  Delay: %1 s").arg(seconds, 0, 'f', 3));

    const int chunkMs = 100;
    int elapsed = 0;
    while (elapsed < totalMs) {
        if (m_stopRequested.loadAcquire()) return false;
        const int wait = qMin(chunkMs, totalMs - elapsed);
        QThread::msleep(static_cast<unsigned long>(wait));
        elapsed += wait;
    }
    return true;
}

bool Page5TestWorker::executeWriteOscilloscope(const QVariantMap& cfg)
{
    Oscilloscope* scope = m_context.scope;
    if (!scope) {
        emit logMessage("  Write Oscilloscope: no oscilloscope connected");
        return false;
    }

    // ── 單位字串轉 double（秒/伏特）────────────────────────────
    // 範例："200ms"→0.2, "1us"→1e-6, "100mV"→0.1, "5V"→5.0
    auto parseUV = [](const QString& s, double def = 0.0) -> double {
        const QString t = s.trimmed().toLower();
        bool ok = false;
        double v = 0.0;
        if      (t.endsWith("ns"))  { v = t.chopped(2).toDouble(&ok); return ok ? v * 1e-9 : def; }
        else if (t.endsWith("us"))  { v = t.chopped(2).toDouble(&ok); return ok ? v * 1e-6 : def; }
        else if (t.endsWith("ms"))  { v = t.chopped(2).toDouble(&ok); return ok ? v * 1e-3 : def; }
        else if (t.endsWith("mv"))  { v = t.chopped(2).toDouble(&ok); return ok ? v * 1e-3 : def; }
        else if (t.endsWith('s'))   { v = t.chopped(1).toDouble(&ok); return ok ? v : def; }
        else if (t.endsWith('v'))   { v = t.chopped(1).toDouble(&ok); return ok ? v : def; }
        v = t.toDouble(&ok);
        return ok ? v : def;
    };

    // ignore_basic（GenericOscWriteDialog）同時涵蓋 Horizontal 與 Acquire
    const bool skipHorz = cfg.value("ignore_horizontal", false).toBool()
                       || cfg.value("ignore_basic",      false).toBool();
    const bool skipAcq  = cfg.value("ignore_acquire",    false).toBool()
                       || cfg.value("ignore_basic",      false).toBool();
    const bool skipCh   = cfg.value("ignore_channels",   false).toBool();
    const bool skipTrig = cfg.value("ignore_trigger",    false).toBool();

    // ── 1. Horizontal / Timescale ─────────────────────────────
    if (skipHorz) {
        emit logMessage("  Horizontal: ignored");
    } else {
        const QString tsStr = cfg.value("timescale").toString();
        if (!tsStr.isEmpty()) {
            const double ts = parseUV(tsStr, 0.2);
            scope->setTimebase(ts);
            emit logMessage(QString("  Timescale: %1 → %2 s/div").arg(tsStr).arg(ts));
        }
        if (cfg.contains("horz_pos"))
            scope->setHorizontalPosition(cfg.value("horz_pos").toDouble());
    }

    // ── 2. Acquire Mode ───────────────────────────────────────
    if (skipAcq) {
        emit logMessage("  Acquire: ignored");
    } else {
        const QString acqMode = cfg.value("acq_mode").toString();
        if (!acqMode.isEmpty()) {
            scope->setAcquisitionMode(acqMode);
            emit logMessage(QString("  Acq Mode: %1").arg(acqMode));
        }
    }

    // ── 3. Trigger (Edge) ─────────────────────────────────────
    if (skipTrig) {
        emit logMessage("  Trigger: ignored");
    } else {
        const QString trigSrc  = cfg.value("trig_source").toString();
        const QString trigEdge = cfg.value("trig_edge").toString();
        if (!trigSrc.isEmpty() || !trigEdge.isEmpty()) {
            scope->setTriggerType("EDGE");
            if (!trigSrc.isEmpty()) {
                scope->setTriggerSource(trigSrc);
                emit logMessage(QString("  Trigger Source: %1").arg(trigSrc));
            }
            if (!trigEdge.isEmpty()) {
                scope->setTriggerSlope(trigEdge);
                emit logMessage(QString("  Trigger Edge: %1").arg(trigEdge));
            }
        }
    }

    // ── 4. Channels CH1~CH4 ───────────────────────────────────
    if (skipCh) {
        emit logMessage("  Channels: ignored");
    } else {
        for (int ch = 1; ch <= 4; ++ch) {
            const QString pfx = QString("ch%1_").arg(ch);
            if (!cfg.contains(pfx + "enabled")) continue;

            const bool    en  = cfg.value(pfx + "enabled", ch == 1).toBool();
            const QString sc  = cfg.value(pfx + "scale",    "100mV").toString();
            const QString cpl = cfg.value(pfx + "coupling", "DC").toString();
            const double  pos = cfg.value(pfx + "position", 0.0).toDouble();

            scope->enableChannel(ch, en);
            if (en) {
                scope->setChannelScale(ch, parseUV(sc, 0.1));
                scope->setChannelCoupling(ch, cpl);
                scope->setChannelPosition(ch, pos);
            }
        }
    }

    emit logMessage("  Write Oscilloscope: done");
    return true;
}

// ─────────────────────────────────────────────
//  doDelay: interruptible sleep helper
// ─────────────────────────────────────────────
static bool doDelay(int ms, QAtomicInt& stop)
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

bool Page5TestWorker::executeTurnOn(const QVariantMap& cfg)
{
    const Page1Config cfg1         = m_context.page1;
    const QString     inputLbl     = cfg.value("input_label").toString();
    const int         loadIdx      = cfg.value("load_index",      -1).toInt();
    const int         dischargeIdx = cfg.value("discharge_index", -1).toInt();
    const int         timeoutMs    = cfg.value("trig_timeout_ms", 10000).toInt();

    // ── ① 初始 Relay 放電（bulk cap 歸零）────────────
    if (dischargeIdx >= 0) {
        auto resOn = InstrumentExecutor::runRelay(
            cfg1, m_context.relays, dischargeIdx, RelayAction::RelayOn);
        if (!resOn.success) {
            emit logMessage(QString("  Turn On: discharge relay on failed — %1").arg(resOn.errorMessage));
            return false;
        }
        if (!doDelay(500, m_stopRequested)) return false;
        auto resOff = InstrumentExecutor::runRelay(
            cfg1, m_context.relays, dischargeIdx, RelayAction::RelayOff);
        if (!resOff.success) {
            emit logMessage(QString("  Turn On: discharge relay off failed — %1").arg(resOff.errorMessage));
            return false;
        }
    }

    // ── ② Load on（整個 ratchet 期間持續開著）────────
    auto resLoad = InstrumentExecutor::runLoad(
        cfg1, m_context.loads, loadIdx, m_context.loadMeta, LoadAction::LoadOn);
    if (!resLoad.success) {
        emit logMessage(QString("  Turn On: load failed — %1").arg(resLoad.errorMessage));
        return false;
    }

    Oscilloscope* scope = m_context.scope;

    if (scope) {
        // ── ③④⑤ Ratchet 策略：含電源循環 + scope 量測 ──
        TurnOnRatchetConfig ratchetCfg;
        ratchetCfg.timeoutMs = timeoutMs;
        auto strategy = std::make_unique<TurnOnRatchetStrategy>(
            cfg1, inputLbl,
            m_context.relays, dischargeIdx,
            ratchetCfg);

        emit logMessage(QString("  Turn On: oscilloscope → %1").arg(strategy->name()));
        auto result = strategy->execute(scope, m_stopRequested);

        if (!result.success) {
            emit logMessage(QString("  Turn On: scope failed — %1").arg(result.errorMessage));
            return false;
        }
        if (!result.errorMessage.isEmpty())
            emit logMessage(QString("  Turn On: scope warning — %1").arg(result.errorMessage));

        for (const auto& ch : result.channels) {
            emit logMessage(QString("    CH%1  Max=%2  Min=%3  RMS=%4  Mean=%5")
                .arg(ch.channel)
                .arg(ch.maxVal, 0, 'f', 4)
                .arg(ch.minVal, 0, 'f', 4)
                .arg(ch.rms,    0, 'f', 4)
                .arg(ch.mean,   0, 'f', 4));
        }
    } else {
        // ── scope 未連線：不上電 ─────────────────────
        auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOff);
        if (!resIn.success) {
            emit logMessage(QString("  Turn On: input failed — %1").arg(resIn.errorMessage));
            return false;
        }
    }

    emit logMessage(QString("  Turn On: done (input=%1, load=%2)").arg(inputLbl).arg(loadIdx));
    return true;
}

bool Page5TestWorker::executeTurnOff(const QVariantMap& cfg)
{
    const Page1Config cfg1     = m_context.page1;
    const QString     inputLbl = cfg.value("input_label").toString();
    const int         loadIdx  = cfg.value("load_index", -1).toInt();
    const int         delayMs  = cfg.value("delay_ms", 5000).toInt();

    auto resLoad = InstrumentExecutor::runLoad(
        cfg1, m_context.loads, loadIdx, m_context.loadMeta, LoadAction::LoadOff);
    if (!resLoad.success) {
        emit logMessage(QString("  Turn Off: load failed — %1").arg(resLoad.errorMessage));
        return false;
    }

    if (delayMs > 0 && !doDelay(delayMs, m_stopRequested))
        return false;

    auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOff);
    if (!resIn.success) {
        emit logMessage(QString("  Turn Off: input failed — %1").arg(resIn.errorMessage));
        return false;
    }

    emit logMessage("  Turn Off: done");
    return true;
}

bool Page5TestWorker::executeRelay(const QVariantMap& cfg)
{
    const Page1Config cfg1     = m_context.page1;
    const int         relayIdx = cfg.value("relay_index", -1).toInt();

    auto res = InstrumentExecutor::runRelay(
        cfg1, m_context.relays, relayIdx, RelayAction::RelayOn);
    if (!res.success) {
        emit logMessage(QString("  Relay: failed - %1").arg(res.errorMessage));
        return false;
    }

    emit logMessage(QString("  Relay: done (idx=%1)").arg(relayIdx));
    return true;
}

bool Page5TestWorker::executeStaticTest(const QVariantMap& cfg)
{
    const Page1Config cfg1     = m_context.page1;
    const QString     inputLbl = cfg.value("input_label").toString();
    const int         loadIdx  = cfg.value("load_index", -1).toInt();
    const int         delayMs  = cfg.value("delay_ms", 5000).toInt();

    // ── 1. 電源開啟 ───────────────────────────────────
    auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOn);
    if (!resIn.success) {
        emit logMessage(QString("  Static Test: input failed — %1").arg(resIn.errorMessage));
        return false;
    }

    // ── 2. 負載開啟 ───────────────────────────────────
    auto resLoad = InstrumentExecutor::runLoad(
        cfg1, m_context.loads, loadIdx, m_context.loadMeta, LoadAction::LoadOn);
    if (!resLoad.success) {
        emit logMessage(QString("  Static Test: load failed — %1").arg(resLoad.errorMessage));
        return false;
    }

    // ── 3. 穩定等待 ───────────────────────────────────
    emit logMessage(QString("  Static Test: settling for %1 ms").arg(delayMs));
    if (!doDelay(delayMs, m_stopRequested))
        return false;

    // ── 4. 示波器量測 ─────────────────────────────────
    Oscilloscope* scope = m_context.scope;
    if (scope) {
        std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(
            OscilloscopeStrategyFactory::create("Static Test"));
        emit logMessage(QString("  Static Test: oscilloscope → %1").arg(strategy->name()));
        auto result = strategy->execute(scope, m_stopRequested);
        if (!result.success) {
            emit logMessage(QString("  Static Test: scope failed — %1").arg(result.errorMessage));
            return false;
        }
        for (const auto& ch : result.channels) {
            emit logMessage(QString("    CH%1  Max=%2  Min=%3  RMS=%4  Mean=%5")
                .arg(ch.channel)
                .arg(ch.maxVal, 0, 'f', 4)
                .arg(ch.minVal, 0, 'f', 4)
                .arg(ch.rms,    0, 'f', 4)
                .arg(ch.mean,   0, 'f', 4));
        }
    }

    emit logMessage("  Static Test: done");
    return true;
}

bool Page5TestWorker::executeDynamicTest(const QVariantMap& cfg)
{
    const Page1Config cfg1      = m_context.page1;
    const QString     inputLbl  = cfg.value("input_label").toString();
    const int         dyLoadIdx = cfg.value("dyload_index", -1).toInt();
    const int         delayMs   = cfg.value("delay_ms", 5000).toInt();

    // ── 1. 電源開啟 ───────────────────────────────────
    auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOn);
    if (!resIn.success) {
        emit logMessage(QString("  Dynamic Test: input failed — %1").arg(resIn.errorMessage));
        return false;
    }

    // ── 2. 動態負載開啟 ───────────────────────────────
    auto resDy = InstrumentExecutor::runDyLoad(
        cfg1, m_context.dynamics, dyLoadIdx, m_context.dynamicMeta, DyLoadAction::DyLoadOn);
    if (!resDy.success) {
        emit logMessage(QString("  Dynamic Test: dyload failed — %1").arg(resDy.errorMessage));
        return false;
    }

    // ── 3. 穩定等待 ───────────────────────────────────
    emit logMessage(QString("  Dynamic Test: settling for %1 ms").arg(delayMs));
    if (!doDelay(delayMs, m_stopRequested))
        return false;

    // ── 4. 示波器量測 ─────────────────────────────────
    Oscilloscope* scope = m_context.scope;
    if (scope) {
        std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(
            OscilloscopeStrategyFactory::create("Dynamic Test"));
        emit logMessage(QString("  Dynamic Test: oscilloscope → %1").arg(strategy->name()));
        auto result = strategy->execute(scope, m_stopRequested);
        if (!result.success) {
            emit logMessage(QString("  Dynamic Test: scope failed — %1").arg(result.errorMessage));
            return false;
        }
        for (const auto& ch : result.channels) {
            emit logMessage(QString("    CH%1  Max=%2  Min=%3  RMS=%4  Mean=%5")
                .arg(ch.channel)
                .arg(ch.maxVal, 0, 'f', 4)
                .arg(ch.minVal, 0, 'f', 4)
                .arg(ch.rms,    0, 'f', 4)
                .arg(ch.mean,   0, 'f', 4));
        }
    }

    emit logMessage("  Dynamic Test: done");
    return true;
}

bool Page5TestWorker::executeTurnOnThenShort(const QVariantMap& cfg)
{
    const Page1Config cfg1     = m_context.page1;
    const QString     inputLbl = cfg.value("input_label").toString();
    const int         loadIdx  = cfg.value("load_index", -1).toInt();
    const int         relayIdx = cfg.value("relay_index", -1).toInt();
    const int         delayMs  = cfg.value("delay_ms", 5000).toInt();

    auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOn);
    if (!resIn.success) { emit logMessage(QString("  Turn On Then Short: input failed — %1").arg(resIn.errorMessage)); return false; }

    auto resLoad = InstrumentExecutor::runLoad(
        cfg1, m_context.loads, loadIdx, m_context.loadMeta, LoadAction::LoadOn);
    if (!resLoad.success) { emit logMessage(QString("  Turn On Then Short: load failed — %1").arg(resLoad.errorMessage)); return false; }

    if (delayMs > 0 && !doDelay(delayMs, m_stopRequested))
        return false;

    auto resRelay = InstrumentExecutor::runRelay(
        cfg1, m_context.relays, relayIdx, RelayAction::RelayOn);
    if (!resRelay.success) { emit logMessage(QString("  Turn On Then Short: relay failed — %1").arg(resRelay.errorMessage)); return false; }

    emit logMessage("  Turn On Then Short: done");
    return true;
}

bool Page5TestWorker::executeShortThenTurnOn(const QVariantMap& cfg)
{
    const Page1Config cfg1     = m_context.page1;
    const QString     inputLbl = cfg.value("input_label").toString();
    const int         loadIdx  = cfg.value("load_index", -1).toInt();
    const int         relayIdx = cfg.value("relay_index", -1).toInt();
    const int         delayMs  = cfg.value("delay_ms", 5000).toInt();

    auto resRelay = InstrumentExecutor::runRelay(
        cfg1, m_context.relays, relayIdx, RelayAction::RelayOn);
    if (!resRelay.success) { emit logMessage(QString("  Short Then Turn On: relay failed — %1").arg(resRelay.errorMessage)); return false; }

    auto resIn = InstrumentExecutor::runInput(cfg1, inputLbl, InputAction::PowerOn);
    if (!resIn.success) { emit logMessage(QString("  Short Then Turn On: input failed — %1").arg(resIn.errorMessage)); return false; }

    auto resLoad = InstrumentExecutor::runLoad(
        cfg1, m_context.loads, loadIdx, m_context.loadMeta, LoadAction::LoadOn);
    if (!resLoad.success) { emit logMessage(QString("  Short Then Turn On: load failed — %1").arg(resLoad.errorMessage)); return false; }

    if (delayMs > 0 && !doDelay(delayMs, m_stopRequested))
        return false;

    emit logMessage("  Short Then Turn On: done");
    return !m_stopRequested.loadAcquire();
}
