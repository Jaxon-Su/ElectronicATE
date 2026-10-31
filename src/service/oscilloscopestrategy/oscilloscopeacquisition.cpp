#include "oscilloscopeacquisition.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <exception>

namespace
{
struct MarginAdjustment {
    ScopeVerticalRange range;
    double scale = 0;
    double anchor = 0;
    double value = 0;
    double position(double actualScale) const { return anchor - (value-range.center)/actualScale; }
};

QString triggerStateName(ScopeTriggerState state)
{
    switch (state) {
    case ScopeTriggerState::Arming: return "ARMED";
    case ScopeTriggerState::Ready: return "READY";
    case ScopeTriggerState::Triggered: return "TRIGGER";
    case ScopeTriggerState::Automatic: return "AUTO";
    case ScopeTriggerState::Saving: return "SAVE";
    case ScopeTriggerState::Invalid: return "Invalid";
    }
    return "Invalid";
}

QString exceptionMessage(const std::exception_ptr &error)
{
    try { std::rethrow_exception(error); }
    catch (const std::exception &e) { return QString::fromUtf8(e.what()); }
    catch (...) { return "Unexpected acquisition error"; }
}

double readReadyMeasurement(IScopeMeasurement *scope, QAtomicInt &stop, int channel, ScopeMeasurement type,
                            int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    double value = std::numeric_limits<double>::quiet_NaN();
    if (timeoutMs <= 0)
        return value;
    do {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        value = OscilloscopeAcquisition::readMeasurementChecked(scope, channel, type);
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (std::isfinite(value) && std::abs(value) < 1e20)
            return value;
        const auto remaining = timeoutMs - timer.elapsed();
        if (remaining <= 0)
            break;
        QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, remaining)));
    } while (timer.elapsed() < timeoutMs);
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

namespace OscilloscopeAcquisition
{

bool channelEnabledChecked(IScopeMeasurement *scope, int channel)
{
    const bool enabled = scope->isChannelEnabled(channel);
    const QString error = scope->lastError();
    if (!error.isEmpty())
        throw std::runtime_error(
            QString("CH%1 display state query failed: %2").arg(channel).arg(error).toStdString());
    return enabled;
}

double readMeasurementChecked(IScopeMeasurement *scope, int channel, ScopeMeasurement type)
{
    const double value = scope->readMeasurement(channel, type);
    const QString error = scope->lastError();
    if (!error.isEmpty())
        throw std::runtime_error(
            QString("CH%1 measurement communication error: %2").arg(channel).arg(error).toStdString());
    return value;
}

bool waitForReady(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs, QString &error,
                  ScopeTriggerState *rejectedState)
{
    if (rejectedState) *rejectedState = ScopeTriggerState::Invalid;
    if (!scope) {
        error = "No oscilloscope";
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        const auto state = scope->triggerState();
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        if (timer.elapsed() >= timeoutMs)
            break;
        if (state == ScopeTriggerState::Ready)
            return true;
        if (state != ScopeTriggerState::Arming) {
            const QString detail = scope->lastError();
            if (rejectedState && detail.isEmpty()) *rejectedState = state;
            error = "Scope not ready before event: state=" + triggerStateName(state);
            if (!detail.isEmpty()) error += "; " + detail;
            return false;
        }
        QThread::msleep(1);
    }
    error = "Scope trigger readiness timeout";
    return false;
}

AcquisitionResult waitForCapture(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                 int completionTimeoutMs, const std::function<void()>& onRecordStopped)
{
    if (stop.loadAcquire())
        return AcquisitionResult::Cancelled;
    if (!scope)
        return AcquisitionResult::CommError;

    QElapsedTimer timer;
    timer.start();
    bool triggered = false;
    auto triggerState = ScopeTriggerState::Invalid;
    int deadline = timeoutMs;
    bool finalPoll = false;

    while (timeoutMs > 0) {
        // Check once at the deadline as well: a record can complete during the
        // last polling sleep. Do not force STOP and mistake a partial record for a hit.
        if (timer.elapsed() >= deadline) {
            if (finalPoll) break;
            finalPoll = true;
        }
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;

        const auto state = scope->acquisitionState();

        // 通訊期間也可能收到停止要求。
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;

        switch (state) {
        case ScopeAcquisitionState::Invalid:
            return AcquisitionResult::CommError;

        case ScopeAcquisitionState::Completed: {
            const bool late = timer.elapsed() >= deadline;
            // The record is frozen now. Let short-circuit tests de-energize before
            // measurement reads or file transfers, without starting another record.
            if (onRecordStopped) onRecordStopped();
            if (stop.loadAcquire())
                return AcquisitionResult::Cancelled;
            // Confirm that acquisition remains stopped after cleanup. Readiness of
            // measurements and SAVE operations is checked by their callers, not a fixed sleep.
            const auto frozenState = scope->acquisitionState();
            if (stop.loadAcquire())
                return AcquisitionResult::Cancelled;
            if (frozenState == ScopeAcquisitionState::Invalid)
                return AcquisitionResult::CommError;
            if (frozenState != ScopeAcquisitionState::Completed)
                return AcquisitionResult::CaptureTimeout;
            return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
        }

        case ScopeAcquisitionState::Running:
            break;
        }

        triggerState = scope->triggerState();
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;
        if (triggerState == ScopeTriggerState::Invalid)
            return AcquisitionResult::CommError;
        if (triggerState == ScopeTriggerState::Triggered) {
            triggered = true;
            if (completionTimeoutMs > deadline) {
                deadline = completionTimeoutMs;
                finalPoll = false;
            }
        }

        const auto remaining = deadline - timer.elapsed();
        if (remaining <= 0) {
            if (finalPoll) break;
            continue;
        }

        QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, remaining)));
    }

    if (triggered || triggerState != ScopeTriggerState::Ready)
        return AcquisitionResult::CaptureTimeout;
    return AcquisitionResult::NoTrigger;
}
} // namespace OscilloscopeAcquisition
bool OscilloscopeAcquisition::expandVerticalRange(IScopeMeasurement *scope, QAtomicInt &stop,
                                                    int &adjustments, VerticalScaleState *vertical,
                                                    const QString &phase, bool completionTimedOut,
                                                    QDeadlineTimer phaseDeadline, RecordExtremum *extremum)
{
    auto check = [&] {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    auto checkPhase = [&] {
        check();
        if (phaseDeadline.hasExpired())
            throw std::runtime_error((phase + " phase time limit reached (timeout): NA").toStdString());
    };
    auto readExtremum = [&](int channel, ScopeMeasurement type) {
        checkPhase();
        const int budget = phaseDeadline.isForever() ? 5000
            : int(qMin<qint64>(5000, phaseDeadline.remainingTime()));
        return readReadyMeasurement(scope, stop, channel, type, budget);
    };
    auto valid = [](double v) { return std::isfinite(v) && std::abs(v) < 1e20; };
    auto setScale = [&](int channel, double previous, double target) {
        checkPhase();
        if (adjustments >= 8)
            throw std::runtime_error("Vertical scale adjustment limit reached");
        if (!valid(target) || target <= 0)
            throw std::runtime_error("Invalid calculated channel scale");
        if (vertical && vertical->options.beforeChange)
            vertical->options.beforeChange(channel, previous, target);
        checkPhase();
        const double actual = setScaleChecked(scope, stop, channel, target, true, phaseDeadline);
        if (vertical && vertical->options.log)
            vertical->options.log(QString("CH%1 Scale readback: requested=%2 actual=%3 units/div")
                .arg(channel).arg(target,0,'g',12).arg(actual,0,'g',12));
        return actual;
    };
    ScopeCaptureRecord record;
    record.phase = phase;
    // Establish the channel list before clipping/measurement queries. A diagnostic
    // transfer must not depend on those queries succeeding.
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        check();
        if (channelEnabledChecked(scope, ch)) {
            OscChannelMeasure value;
            value.channel = ch;
            record.channels.append(value);
        }
    }
    QVector<int> clipped;
    if (extremum) {
        extremum->clipped = false;
        extremum->value = std::numeric_limits<double>::quiet_NaN();
    }
    QMap<int, QPair<double, double>> expansions;
    QMap<int, MarginAdjustment> margins;
    const bool margin = vertical && vertical->options.margin;
    std::exception_ptr queryFailure;
    try {
        for (const auto &value : record.channels) {
            // Late completion still gets its diagnostic record below.
            if (!completionTimedOut) checkPhase();
            else check();
            if (scope->isClipping(value.channel)) clipped.append(value.channel);
        }
        record.clipped = !clipped.isEmpty();
        const bool capture = vertical && vertical->captureRecord && vertical->readCaptureMeasurements;
        if ((capture || margin) && !completionTimedOut) {
            for (auto &value : record.channels) {
                const bool adjustMargin = margin && !clipped.contains(value.channel);
                if (!capture && !adjustMargin) continue;
                value.maxVal = readExtremum(value.channel, ScopeMeasurement::Maximum);
                value.minVal = readExtremum(value.channel, ScopeMeasurement::Minimum);
                checkPhase();
                if (capture && vertical->includeRmsMean) {
                    value.rms = readMeasurementChecked(scope, value.channel, ScopeMeasurement::Rms);
                    checkPhase();
                    value.mean = readMeasurementChecked(scope, value.channel, ScopeMeasurement::Mean);
                    checkPhase();
                }
                if (adjustMargin) {
                    checkPhase();
                    ScopeVerticalRange range;
                    QString error;
                    if (!scope->readVerticalRange(value.channel, range, error))
                        throw std::runtime_error(QString("CH%1 vertical range: %2").arg(value.channel).arg(error).toStdString());
                    checkPhase();
                    if (!valid(range.scale) || range.scale <= 0 || !valid(range.center) ||
                        !valid(range.divisions) || range.divisions <= 0 || !valid(range.position) ||
                        !valid(range.minimumPosition) || !valid(range.maximumPosition) ||
                        range.minimumPosition >= -range.divisions*0.4 ||
                        range.maximumPosition <= range.divisions*0.4 ||
                        range.position < range.minimumPosition || range.position > range.maximumPosition ||
                        !valid(value.maxVal) || !valid(value.minVal) || value.maxVal < value.minVal)
                        throw std::runtime_error(QString("CH%1 invalid vertical range or extrema for 10% margin").arg(value.channel).toStdString());
                    const double top = (value.maxVal-range.center)/range.scale + range.position;
                    const double bottom = (value.minVal-range.center)/range.scale + range.position;
                    if (top <= range.divisions*0.4 + 1e-9 && bottom >= -range.divisions*0.4 - 1e-9)
                        continue;
                    // Position affects display only. ADC clipping above still uses x2.
                    const double span = value.maxVal-value.minVal;
                    const double edge = range.divisions*0.395;
                    const bool flat = span <= range.scale*0.1;
                    MarginAdjustment plan{range, std::max(range.scale,span/(2*edge)),
                                          0, value.minVal+span/2};
                    if (flat && value.minVal > 0) { plan.anchor=edge; plan.value=value.maxVal; }
                    else if (flat && value.maxVal < 0) { plan.anchor=-edge; plan.value=value.minVal; }
                    const double delta = plan.value-range.center;
                    // Leave the ADC Offset unchanged; expand only when span or Position limits require it.
                    plan.scale = std::max(plan.scale, delta >= 0 ? delta/(plan.anchor-range.minimumPosition)
                                                               : -delta/(range.maximumPosition-plan.anchor));
                    if (!valid(plan.scale) || plan.scale <= 0)
                        throw std::runtime_error("Invalid calculated margin scale");
                    margins.insert(value.channel,plan);
                    if (plan.scale > range.scale*(1+1e-9))
                        expansions.insert(value.channel,{range.scale,plan.scale});
                    if (vertical->options.log)
                        vertical->options.log(QString("CH%1 10% display margin: %2; Max=%3 Min=%4 Offset=%5 Position=%6 div; requested Scale=%7")
                            .arg(value.channel).arg(flat ? "flat edge/zero center" : "waveform center")
                            .arg(value.maxVal,0,'g',12).arg(value.minVal,0,'g',12).arg(range.center,0,'g',12)
                            .arg(range.position,0,'g',12).arg(plan.scale,0,'g',12));
                }
            }
        }
    } catch (...) { queryFailure = std::current_exception(); }
    check(); // Stop and failed short-circuit exit never start a diagnostic transfer.
    if (vertical && vertical->captureRecord) {
        try { vertical->captureRecord(record); }
        catch (...) {
            if (queryFailure)
                throw std::runtime_error((exceptionMessage(queryFailure) + "; Capture: " +
                                          exceptionMessage(std::current_exception())).toStdString());
            throw;
        }
    }
    check();
    if (queryFailure) std::rethrow_exception(queryFailure);
    if (completionTimedOut) throw std::runtime_error("Acquisition completed after timeout");
    checkPhase();
    if (extremum) {
        extremum->clipped = clipped.contains(extremum->channel);
        if (!extremum->clipped) {
            // Keep Baseline's single extremum read after diagnostic Capture, but
            // before scale writes invalidate the frozen record's interpretation.
            extremum->value = readMeasurementChecked(scope, extremum->channel, extremum->type);
            checkPhase();
            if (!valid(extremum->value))
                throw std::runtime_error((phase + " measurement: NA").toStdString());
        }
    }
    for (int ch : clipped) {
        checkPhase();
        const double previous = scope->getChannelScale(ch);
        const QString error = scope->lastError();
        if (!error.isEmpty()) throw std::runtime_error(("Channel scale query: " + error).toStdString());
        if (!valid(previous) || previous <= 0)
            throw std::runtime_error("Clipped waveform: invalid channel scale");
        expansions.insert(ch, {previous, previous * 2});
    }
    const bool changed = !expansions.isEmpty() || !margins.isEmpty();
    if (changed && adjustments >= 8)
        throw std::runtime_error("Vertical scale adjustment limit reached");
    QMap<int,double> actualScales;
    for (auto it = expansions.cbegin(); it != expansions.cend(); ++it)
        actualScales.insert(it.key(),setScale(it.key(), it.value().first, it.value().second));
    for (auto it = margins.cbegin(); it != margins.cend(); ++it) {
        checkPhase();
        const auto &plan = it.value();
        const double target = std::clamp(plan.position(actualScales.value(it.key(),plan.range.scale)),
                                         plan.range.minimumPosition,plan.range.maximumPosition);
        if (std::abs(target-plan.range.position) <= 1e-9) continue;
        if (vertical->options.beforePositionChange)
            vertical->options.beforePositionChange(it.key(),plan.range.position,target);
        checkPhase();
        const double actual = setPositionChecked(scope,stop,it.key(),target,phaseDeadline);
        if (vertical->options.log)
            vertical->options.log(QString("CH%1 Position readback: requested=%2 actual=%3 div")
                .arg(it.key()).arg(target,0,'g',12).arg(actual,0,'g',12));
    }
    if (changed) ++adjustments;
    return changed;
}

OscilloscopeAcquisition::AcquisitionResult OscilloscopeAcquisition::acquireAutoRecord(
    IScopeMeasurement *scope, QAtomicInt &stop, int phaseRemainingMs, QString &error, const ReferenceLog &log)
{
    error.clear();
    auto report = [&](const QString &message) { if (log) log(message); };
    if (stop.loadAcquire()) { error = "Stopped by user"; return AcquisitionResult::Cancelled; }
    if (!scope || phaseRemainingMs <= 0) {
        error = "AUTO reference phase timeout: NA"; return AcquisitionResult::CaptureTimeout;
    }
    QElapsedTimer timer; timer.start();
    QString state;
    auto abort = [&](AcquisitionResult result) {
        QString cleanup;
        try {
            if (!scope->stopAcquisition(cleanup)) error += "; AUTO STOP: " + cleanup;
        } catch (...) { error += "; AUTO STOP exception"; }
        report("AUTO reference: " + error);
        return result;
    };
    try {
        int recordBudget = 0;
        report("AUTO reference: starting fresh RUNSTOP acquisition");
        if (!scope->beginAutoReference(recordBudget,error)) return abort(AcquisitionResult::CommError);
        if (recordBudget <= 0 || recordBudget > 60000) {
            error = "Invalid AUTO reference timeout budget"; return abort(AcquisitionResult::CommError);
        }
        const int budget = qMin(phaseRemainingMs,recordBudget);
        report(QString("AUTO reference: wait limit=%1 ms (remaining phase=%2 ms)").arg(budget).arg(phaseRemainingMs));
        QString previous;
        qint64 lastLog = -1000;
        for (;;) {
            if (stop.loadAcquire()) { error = "Stopped by user"; return abort(AcquisitionResult::Cancelled); }
            const auto status = scope->pollAutoReference(state,error);
            if (stop.loadAcquire()) { error = "Stopped by user"; return abort(AcquisitionResult::Cancelled); }
            if (state != previous || timer.elapsed()-lastLog >= 1000) {
                report(QString("AUTO reference: %1; elapsed=%2 ms").arg(state).arg(timer.elapsed()));
                previous = state; lastLog = timer.elapsed();
            }
            if (status == ScopeReferenceStatus::Error) {
                error = "AUTO reference communication/state error: " + error + "; " + state;
                return abort(AcquisitionResult::CommError);
            }
            if (status == ScopeReferenceStatus::Completed) {
                const bool late = timer.elapsed() >= budget;
                report(late ? "AUTO reference: record frozen after timeout (diagnostic only)"
                            : "AUTO reference: fresh record frozen");
                return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
            }
            if (timer.elapsed() >= budget) {
                error = QString("AUTO reference timeout: NA after %1 ms; %2").arg(timer.elapsed()).arg(state);
                return abort(AcquisitionResult::CaptureTimeout);
            }
            QThread::msleep(static_cast<unsigned long>(std::clamp<qint64>(budget-timer.elapsed(),0,20)));
        }
    } catch (const std::exception &ex) { error = QString("AUTO reference error: ") + ex.what(); }
      catch (...) { error = "Unexpected AUTO reference error"; }
    return abort(AcquisitionResult::CommError);
}

OscMeasureResult OscilloscopeAcquisition::captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop,
                                                               int timeoutMs, VerticalScaleState *vertical,
                                                               ReferenceMetrics metrics, const ReferenceLog &log,
                                                               bool detailedLog)
{
    OscMeasureResult result;
    QString step;
    auto detail = [&](const QString &message) {
        if (detailedLog && log) log("AUTO reference: " + message);
    };
    auto begin = [&](const QString &name) {
        step = name;
        detail(step + " begin");
    };
    if (!scope || timeoutMs <= 0) {
        result.errorMessage = "Invalid AUTO reference acquisition";
        return result;
    }
    QElapsedTimer referenceTimer;
    referenceTimer.start();
    auto check = [&] {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (referenceTimer.elapsed() >= timeoutMs)
            throw std::runtime_error("AUTO reference phase timeout: NA");
    };
    try {
        begin("channel validation");
        check();
        QString error;
        const int channel = scope->triggerChannel();
        if (channel < 1 || channel > scope->getTotalChannel() || !channelEnabledChecked(scope, channel))
            throw std::runtime_error("AUTO reference requires an enabled analog trigger channel");
        const double level = scope->getChannelScale(channel) * 0.04;
        if (!std::isfinite(level) || level <= 0 || level >= 1e19)
            throw std::runtime_error("Invalid channel scale for AUTO reference trigger level");
        detail(step + " complete");
        auto configure = [&](auto operation) {
            check();
            if (!operation(error))
                throw std::runtime_error(error.toStdString());
        };
        begin("trigger configuration");
        configure([&](QString &e) { return scope->selectEdgeTrigger(e); });
        configure([&](QString &e) { return scope->setEdgeSlope(ScopeTriggerSlope::Rise, e); });
        configure([&](QString &e) { return scope->setChannelTriggerLevel(channel, level, e); });
        configure([&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Auto, e); });
        check();
        detail(step + " complete");
        VerticalScaleState localVertical;
        if (!vertical)
            vertical = &localVertical;
        int adjustments = 0;
        for (;;) {
            if (referenceTimer.elapsed() >= timeoutMs)
                throw std::runtime_error("AUTO reference timeout: NA");
            begin("acquisition");
            const auto state = acquireAutoRecord(scope, stop,
                qMax(0, timeoutMs - int(referenceTimer.elapsed())), error, log);
            if (!hasCompletedRecord(state))
                throw std::runtime_error(error.toStdString());
            detail(step + " complete");
            // Reading or archiving a completed record can cross the phase deadline.
            // Archive the frozen record before enforcing that deadline.
            begin("record checks / Capture");
            if (expandVerticalRange(scope, stop, adjustments, vertical, "Auto reference",
                                      state == AcquisitionResult::CompletedAfterTimeout,
                                      QDeadlineTimer(qMax(0, timeoutMs - int(referenceTimer.elapsed()))))) {
                detail(step + " complete; new acquisition required");
                continue;
            }
            detail(step + " complete");
            check();
            break;
        }
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            begin(QString("CH%1 display/clipping check").arg(ch));
            check();
            if (!channelEnabledChecked(scope, ch)) {
                detail(step + " complete; disabled");
                continue;
            }
            if (scope->isClipping(ch))
                throw std::runtime_error("AUTO reference clipped; adjust channel scale");
            detail(step + " complete");
            auto read = [&](ScopeMeasurement type, const char *name) {
                begin(QString("CH%1 %2 read").arg(ch).arg(QString::fromLatin1(name)));
                check();
                const double value =
                    (type == ScopeMeasurement::Maximum || type == ScopeMeasurement::Minimum)
                        ? readReadyMeasurement(scope, stop, ch, type,
                                               qMin(5000, qMax(0, timeoutMs - int(referenceTimer.elapsed()))))
                        : readMeasurementChecked(scope, ch, type);
                check();
                detail(step + " complete");
                return std::isfinite(value) && std::abs(value) < 1e30
                           ? value
                           : std::numeric_limits<double>::quiet_NaN();
            };
            OscChannelMeasure value;
            value.channel = ch;
            value.maxVal = read(ScopeMeasurement::Maximum, "Maximum");
            value.minVal = read(ScopeMeasurement::Minimum, "Minimum");
            // Reference results can request a quartet independently of Capture's contract.
            if (vertical->includeRmsMean || metrics == ReferenceMetrics::Quartet) {
                value.rms = read(ScopeMeasurement::Rms, "RMS");
                value.mean = read(ScopeMeasurement::Mean, "Mean");
            }
            if (!std::isfinite(value.maxVal) || !std::isfinite(value.minVal) || value.maxVal < value.minVal)
                throw std::runtime_error("AUTO reference Max/Min: NA");
            result.channels.append(value);
        }
        if (result.channels.isEmpty())
            throw std::runtime_error("AUTO reference has no enabled channels");
        result.success = true;
    } catch (const std::exception &ex) {
        result.errorMessage = QString::fromUtf8(ex.what());
    } catch (...) {
        result.errorMessage = "Unexpected AUTO reference error";
    }
    if (!result.success) detail("failed during " + step + ": " + result.errorMessage);
    auto cleanup = [&](const QString &name, auto operation) {
        begin(name);
        QString error;
        try {
            if (operation(error)) {
                detail(step + " complete");
                return;
            }
        } catch (...) {
            error = "cleanup exception";
        }
        result.success = false;
        result.cleanupFailed = true;
        result.errorMessage += "; AUTO cleanup: " + error;
        detail(step + " failed: " + error);
    };
    cleanup("cleanup STOP", [&](QString &e) { return scope->stopAcquisition(e); });
    cleanup("cleanup NORMAL", [&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
    if (log) log(result.success ? "AUTO reference complete; returned to STOP/NORMAL"
                               : "AUTO reference failed: " + result.errorMessage);
    if (stop.loadAcquire()) {
        result.success = false;
        result.errorMessage = "Stopped by user";
    }
    return result;
}
