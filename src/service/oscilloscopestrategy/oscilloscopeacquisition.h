#pragma once
#include <QAtomicInt>
#include <QMap>
#include <QSet>
#include "ioscilloscopemeasurestrategy.h"
#include "iscopemeasurement.h"

namespace OscilloscopeAcquisition
{
enum class AcquisitionResult {
    Completed,     // 已啟動的 Single 擷取在期限內完成。
    NoTrigger,     // 期限內未完成擷取；不保證完全沒有發生觸發。
    CommError,     // 無示波器、查詢失敗或回應無法辨識。
    Cancelled,     // 使用者要求停止。
    CaptureTimeout // Triggered or not yet ready: never classify as a normal MISS.
};

struct VerticalScaleState {
    OscCaptureObserver captureRecord;
};

// 呼叫前須成功啟動 Single，並確保儀器由呼叫端獨占且存活。
// 僅等待狀態，不負責啟動、停止或釋放儀器；呼叫端處理結果與清理。
// timeoutMs 是擷取輪詢期限；完成後另加 driver 的資料更新等待時間。
// 通訊仍受底層 I/O timeout 約束；外層 phase deadline 仍包含資料更新等待。
// AUTO trigger captures one reference record; always returns to STOP/NORMAL. Does not AutoSet.
OscMeasureResult captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                      VerticalScaleState *vertical = nullptr);

// Recover clipping with 2x expansion; preserve Position and Offset.
// Unclipped records are never automatically fitted or centered.
// Returns true when the caller must acquire a NEW record before measuring.
bool expandClippedChannels(IScopeMeasurement *scope, QAtomicInt &stop, int &adjustments,
                           VerticalScaleState *vertical = nullptr, bool saveCapture = true);

bool waitForReady(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs, QString &error);
AcquisitionResult waitForCapture(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                 int completionTimeoutMs = -1);
} // namespace OscilloscopeAcquisition
