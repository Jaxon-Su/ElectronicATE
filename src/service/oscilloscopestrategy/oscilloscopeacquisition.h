#pragma once
#include <QAtomicInt>
#include <QDeadlineTimer>
#include <QMap>
#include <QSet>
#include <functional>
#include <limits>
#include "oscmeasurementresult.h"
#include "osccaptureobserver.h"
#include "iscopemeasurement.h"
#include "verticalscaleoptions.h"
#include "verticalscalecontrol.h"

namespace OscilloscopeAcquisition
{
enum class AcquisitionResult {
    Completed,     // 已啟動的擷取在期限內完成，記錄已凍結。
    CompletedAfterTimeout, // 完成回覆晚到：先保存診斷，再回報超時；不得納入成功結果。
    NoTrigger,     // 期限內未完成擷取；不保證完全沒有發生觸發。
    CommError,     // 無示波器、查詢失敗或回應無法辨識。
    Cancelled,     // 使用者要求停止。
    CaptureTimeout // Triggered or not yet ready: never classify as a normal MISS.
};

inline bool hasCompletedRecord(AcquisitionResult result)
{
    return result == AcquisitionResult::Completed || result == AcquisitionResult::CompletedAfterTimeout;
}

struct VerticalScaleState {
    OscCaptureObserver captureRecord;
    bool includeRmsMean = true;
    VerticalScaleOptions options;
    // Sparse diagnostics need the frozen waveform/channel list, without waiting
    // for measurements on a clipped record that will be reacquired.
    bool readCaptureMeasurements = true;
};

// Optional output from the frozen record, before any vertical scale changes.
// A clipped channel has no usable extremum; callers may still retain its trigger level.
struct RecordExtremum {
    int channel;
    ScopeMeasurement type;
    bool clipped = false;
    double value = std::numeric_limits<double>::quiet_NaN();
};

// Preserve legal NA, but capture transport/setup errors before another query clears them.
double readMeasurementChecked(IScopeMeasurement *scope, int channel, ScopeMeasurement type);
// Distinguish a disabled channel from a failed query before any later IO clears it.
bool channelEnabledChecked(IScopeMeasurement *scope, int channel);

// Reference-only AUTO/RUNSTOP: starts a fresh acquisition and freezes a new record.
// Polling is bounded by the device's record budget and the caller's remaining phase;
// synchronous I/O retains its transport timeout. Late records remain diagnostic only.
enum class ReferenceMetrics { CaptureDefault, Quartet };
using ReferenceLog = std::function<void(const QString &)>;
AcquisitionResult acquireAutoRecord(IScopeMeasurement *scope, QAtomicInt &stop, int phaseRemainingMs,
                                     QString &error, const ReferenceLog &log = {});
// Reads the frozen reference and restores STOP/NORMAL. Does not AutoSet.
OscMeasureResult captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                      VerticalScaleState *vertical = nullptr,
                                      ReferenceMetrics metrics = ReferenceMetrics::CaptureDefault,
                                      const ReferenceLog &log = {}, bool detailedLog = false);

// Recover clipping with 2x expansion; preserve Position and Offset.
// Optional 10% top/bottom margin only expands; never centers or shrinks.
// Callers pass their remaining phase deadline. Expiry bounds measurement readiness,
// but a completed record can still be archived before reporting failure.
// Returns true when the caller must acquire a NEW record before measuring.
bool expandVerticalRange(IScopeMeasurement *scope, QAtomicInt &stop, int &adjustments,
                           VerticalScaleState *vertical = nullptr, const QString &phase = "Measurement",
                           bool completionTimedOut = false,
                           QDeadlineTimer phaseDeadline = QDeadlineTimer(QDeadlineTimer::Forever),
                           RecordExtremum *extremum = nullptr);

// rejectedState is set only for a timely, error-free non-READY/non-ARMED reply;
// it stays Invalid for cancellation, timeout and communication errors.
bool waitForReady(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs, QString &error,
                  ScopeTriggerState *rejectedState = nullptr);
// Requires an already-started Single; only waits and does not stop/rearm it.
AcquisitionResult waitForCapture(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                 int completionTimeoutMs = -1,
                                 const std::function<void()>& onRecordStopped = {});
} // namespace OscilloscopeAcquisition
