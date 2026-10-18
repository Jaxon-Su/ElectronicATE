#pragma once
#include <QAtomicInt>
#include <QMap>
#include <QSet>
#include <functional>
#include "oscmeasurementresult.h"
#include "osccaptureobserver.h"
#include "iscopemeasurement.h"

namespace OscilloscopeAcquisition
{
enum class AcquisitionResult {
    Completed,     // 已啟動的 Single 擷取在期限內完成。
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
};

// Preserve legal NA, but capture transport/setup errors before another query clears them.
double readMeasurementChecked(IScopeMeasurement *scope, int channel, ScopeMeasurement type);
// Distinguish a disabled channel from a failed query before any later IO clears it.
bool channelEnabledChecked(IScopeMeasurement *scope, int channel);

// 呼叫前須成功啟動 Single，並確保儀器由呼叫端獨占且存活。
// 僅等待狀態，不負責啟動、停止或釋放儀器；呼叫端處理結果與清理。
// timeoutMs 是擷取輪詢期限；晚到的完成記錄仍交給呼叫端保存並判失敗。
// 通訊仍受底層 I/O timeout 約束；不額外加入固定資料更新等待。
// AUTO trigger captures one reference record; always returns to STOP/NORMAL. Does not AutoSet.
enum class ReferenceMetrics { CaptureDefault, Quartet };
OscMeasureResult captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                      VerticalScaleState *vertical = nullptr,
                                      ReferenceMetrics metrics = ReferenceMetrics::CaptureDefault);

// Recover clipping with 2x expansion; preserve Position and Offset.
// Unclipped records are never automatically fitted or centered.
// Returns true when the caller must acquire a NEW record before measuring.
bool expandClippedChannels(IScopeMeasurement *scope, QAtomicInt &stop, int &adjustments,
                           VerticalScaleState *vertical = nullptr, const QString &phase = "Measurement",
                           bool completionTimedOut = false);

bool waitForReady(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs, QString &error);
AcquisitionResult waitForCapture(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                 int completionTimeoutMs = -1,
                                 const std::function<void()>& onRecordStopped = {});
} // namespace OscilloscopeAcquisition
