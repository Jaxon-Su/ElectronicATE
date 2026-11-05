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
    QString placement = "center";
    QString limitReason;
    double unavailableScale = 0;
    double position(double actualScale) const { return anchor - (value-range.center)/actualScale; }
};

MarginAdjustment fitMargin(const ScopeVerticalRange &range, double high, double low,
                           const ChannelFitLimits &limits, bool fits)
{
    const double span = high-low;
    const double edge = range.divisions*0.395;
    MarginAdjustment plan{range, range.scale, 0, low+span/2};
    if (low >= 0 && high > 0) {
        plan.anchor=edge; plan.value=high; plan.placement="upper edge";
    } else if (high <= 0 && low < 0) {
        plan.anchor=-edge; plan.value=low; plan.placement="lower edge";
    }
    const double fraction = span / range.scale / range.divisions;
    if (span == 0) plan.limitReason = "zero span";
    else if (fraction < 0.76 || fraction > 0.8 || (!fits && fraction > 0.79))
        plan.scale = span / (2*edge);

    // The probe Offset stays fixed. The requested display anchor must be
    // reachable at the device's Position limits even for a one-sided signal.
    const double delta = plan.value-range.center;
    const double positionFloor = delta >= 0 ? delta/(plan.anchor-range.minimumPosition)
                                            : -delta/(range.maximumPosition-plan.anchor);
    if (plan.scale < positionFloor) {
        plan.scale = positionFloor;
        plan.limitReason = "Position limit";
    }
    double clippedRequest = limits.clippedScale;
    if (limits.roundedDown.second > 0 && limits.roundedDown.second <= clippedRequest*(1+1e-9))
        clippedRequest = std::max(clippedRequest,limits.roundedDown.first);
    if (plan.scale < range.scale && plan.scale <= clippedRequest*(1+1e-9)) {
        plan.scale = range.scale;
        plan.limitReason = "smaller Scale clipped";
    }
    if (plan.scale < range.scale && plan.scale >= limits.roundedUp.first*(1-1e-9) &&
        range.scale <= limits.roundedUp.second*(1+1e-9)) {
        plan.scale = range.scale;
        plan.limitReason = "device Scale rounding";
    }
    if (!fits && plan.scale > range.scale && qFuzzyCompare(range.scale,limits.roundedDown.second) &&
        plan.scale <= limits.roundedDown.first*(1+1e-9)) {
        // The requested fit rounded below the safe window. Try a coarser
        // setting instead of writing the same unrepresentable request again.
        plan.unavailableScale = plan.scale;
        plan.scale = std::max(plan.scale,range.scale*2);
    }
    return plan;
}

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

} // namespace

namespace OscilloscopeAcquisition {
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
    auto setScale = [&](int channel, double previous, double target, bool fitting) {
        checkPhase();
        if (adjustments >= 8)
            throw std::runtime_error("Vertical scale adjustment limit reached");
        if (!valid(target) || target <= 0)
            throw std::runtime_error("Invalid calculated channel scale");
        if (vertical && vertical->options.beforeChange)
            vertical->options.beforeChange(channel, previous, target);
        checkPhase();
        const double actual = setScaleChecked(scope, stop, channel, target, true, phaseDeadline, fitting ? previous : 0);
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
    if (margin && !vertical->options.history) vertical->options.history = std::make_shared<VerticalFitHistory>();
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
        // A clipped margin record is diagnostic only. Resolve clipping across
        // all enabled channels before waiting for extrema or planning Position.
        if ((capture || margin) && !completionTimedOut && !(margin && record.clipped)) {
            for (auto &value : record.channels) {
                const bool adjustMargin = margin && vertical->fitDisplay && !clipped.contains(value.channel);
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
                    // Position affects display only. ADC clipping above still uses x2.
                    const double span = value.maxVal-value.minVal;
                    const double limit = range.divisions*0.4;
                    const double occupancy = span / range.scale / range.divisions;
                    const bool fits = top <= limit + 1e-9 && bottom >= -limit - 1e-9 &&
                                      occupancy <= 0.8 + 1e-9;
                    const auto plan = fitMargin(range, value.maxVal, value.minVal,
                                                vertical->options.history->value(value.channel), fits);
                    if (!valid(plan.scale) || plan.scale <= 0)
                        throw std::runtime_error("Invalid calculated margin scale");
                    const bool resize = std::abs(plan.scale-range.scale) > range.scale*1e-9;
                    const bool aligned = std::abs(plan.position(range.scale)-range.position)
                                         <= range.divisions*0.005 + 1e-9;
                    if (vertical->options.log)
                        vertical->options.log(QString("CH%1 10% display margin: %2; occupancy=%3% (target 76-80%); bottom=%4 top=%5 div; %6 %7")
                            .arg(value.channel).arg(plan.placement).arg(occupancy*100,0,'g',5)
                            .arg(bottom,0,'g',8).arg(top,0,'g',8)
                            .arg(fits && aligned && !resize ? "accepted" : "adjust").arg(plan.limitReason));
                    if (fits && aligned && !resize) continue;
                    margins.insert(value.channel,plan);
                    if (resize)
                        expansions.insert(value.channel,{range.scale,plan.scale});
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
            if (!valid(extremum->value)) {
                if (!margin || !record.clipped)
                    throw std::runtime_error((phase + " measurement: NA").toStdString());
                extremum->value = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }
    for (int ch : clipped) {
        checkPhase();
        const double previous = scope->getChannelScale(ch);
        const QString error = scope->lastError();
        if (!error.isEmpty()) throw std::runtime_error(("Channel scale query: " + error).toStdString());
        if (!valid(previous) || previous <= 0)
            throw std::runtime_error("Clipped waveform: invalid channel scale");
        if (margin) {
            auto &limits = (*vertical->options.history)[ch];
            limits.clippedScale = std::max(limits.clippedScale,previous);
        }
        expansions.insert(ch, {previous, previous * 2});
    }
    const bool changed = !expansions.isEmpty() || !margins.isEmpty();
    if (changed && adjustments >= 8)
        throw std::runtime_error("Vertical scale adjustment limit reached");
    QMap<int,double> actualScales;
    for (auto it = expansions.cbegin(); it != expansions.cend(); ++it) {
        const int ch = it.key();
        const double target = it.value().second;
        const bool fitting = margins.contains(ch);
        const double previous = it.value().first;
        double actual;
        try {
            actual = setScale(ch,previous,target,fitting);
        } catch (const UnchangedFitScale &e) {
            // A small outward request can round to the current setting. One
            // coarser request must actually move the scale before reacquisition.
            if (!fitting || target <= previous || target >= 2*previous) throw;
            if (vertical->options.log) vertical->options.log(QString::fromUtf8(e.what()) + "; trying a coarser Scale");
            actual = setScale(ch,previous,2*previous,true);
        }
        actualScales.insert(ch,actual);
        if (fitting) {
            auto &limits = (*vertical->options.history)[ch];
            const auto &plan = margins.value(ch);
            if (plan.unavailableScale > 0) limits.roundedUp={plan.unavailableScale,actual};
            else if (actual > target*(1+1e-9)) limits.roundedUp={target,actual};
            if (actual < target*(1-1e-9)) limits.roundedDown={target,actual};
        }
    }
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

OscilloscopeAcquisition::AcquisitionResult OscilloscopeAcquisition::acquireReferenceRecord(
    IScopeMeasurement *scope, QAtomicInt &stop, int phaseRemainingMs, QString &error, const ReferenceLog &log,
    ScopeTriggerMode triggerMode)
{
    error.clear();
    const QString label = triggerMode == ScopeTriggerMode::Auto ? "AUTO reference" : "NORMAL period record";
    auto report = [&](const QString &message) { if (log) log(message); };
    if (stop.loadAcquire()) { error = "Stopped by user"; return AcquisitionResult::Cancelled; }
    if (!scope || phaseRemainingMs <= 0) {
        error = label + " phase timeout: NA"; return AcquisitionResult::CaptureTimeout;
    }
    QElapsedTimer timer; timer.start();
    QString state;
    auto abort = [&](AcquisitionResult result) {
        QString cleanup;
        try {
            if (!scope->stopAcquisition(cleanup)) error += "; STOP: " + cleanup;
        } catch (...) { error += "; STOP exception"; }
        report(label + ": " + error);
        return result;
    };
    try {
        int recordBudget = 0;
        report(label + ": starting fresh RUNSTOP acquisition");
        if (!scope->beginReferenceRecord(triggerMode, recordBudget,error)) return abort(AcquisitionResult::CommError);
        if (recordBudget <= 0 || recordBudget > 60000) {
            error = label + ": invalid timeout budget"; return abort(AcquisitionResult::CommError);
        }
        const int budget = qMin(phaseRemainingMs,recordBudget);
        report(label + QString(": wait limit=%1 ms (remaining phase=%2 ms)").arg(budget).arg(phaseRemainingMs));
        QString previous;
        qint64 lastLog = -1000;
        for (;;) {
            if (stop.loadAcquire()) { error = "Stopped by user"; return abort(AcquisitionResult::Cancelled); }
            const auto status = scope->pollReferenceRecord(state,error);
            if (stop.loadAcquire()) { error = "Stopped by user"; return abort(AcquisitionResult::Cancelled); }
            if (state != previous || timer.elapsed()-lastLog >= 1000) {
                report(label + QString(": %1; elapsed=%2 ms").arg(state).arg(timer.elapsed()));
                previous = state; lastLog = timer.elapsed();
            }
            if (status == ScopeReferenceStatus::Error) {
                error = label + " communication/state error: " + error + "; " + state;
                return abort(AcquisitionResult::CommError);
            }
            if (status == ScopeReferenceStatus::Completed) {
                const bool late = timer.elapsed() >= budget;
                report(label + (late ? ": record frozen after timeout (diagnostic only)" : ": fresh record frozen"));
                return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
            }
            if (timer.elapsed() >= budget) {
                error = label + QString(" timeout: NA after %1 ms; %2").arg(timer.elapsed()).arg(state);
                return abort(AcquisitionResult::CaptureTimeout);
            }
            QThread::msleep(static_cast<unsigned long>(std::clamp<qint64>(budget-timer.elapsed(),0,20)));
        }
    } catch (const std::exception &ex) { error = label + " error: " + ex.what(); }
      catch (...) { error = label + ": unexpected error"; }
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
            const auto state = acquireReferenceRecord(scope, stop,
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
