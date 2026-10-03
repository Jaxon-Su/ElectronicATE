#pragma once
#include <memory>
#include "capturesession.h"
#include <functional>
#include <QString>

class IScopeCapture;

using CaptureFileSelector =
    std::function<QString(const QString& title, const QString& defaultPath, const QString& filter)>;

// 擷取命令執行所需的共享上下文
// 由 ViewModel 組裝後傳入各 CaptureCommand，解耦命令與 ViewModel
struct CaptureContext {
    // 非同步擷取期間保持 oscilloscope 存活（shared_ptr 防 use-after-free）
    std::shared_ptr<IScopeCapture> oscilloscope;

    // Shared exclusivity and shutdown state for all capture commands.
    std::shared_ptr<CaptureSession> captureSession;

    // 預設儲存目錄（上次使用的路徑）
    QString lastSaveDir;

    // Synchronous UI-thread interaction supplied by the View. An absent selector
    // behaves like cancellation, allowing commands to run without Qt Widgets.
    CaptureFileSelector selectSaveFile;
    QString requestSaveFile(const QString& title, const QString& defaultPath, const QString& filter) const
    {
        return selectSaveFile ? selectSaveFile(title, defaultPath, filter) : QString{};
    }

    // 檔案對話框確認後，通知 ViewModel 更新並持久化路徑
    // 在 execute() 內同步呼叫（file dialog 之後、async 之前）
    std::function<void(const QString&)> onSaveDirChanged;

    // Capture 區塊選擇的通道（1-based），單通道 CSV / WFM 必須明確指定。
    // PNG / AllCSV / AllWFM 不使用此欄位；不連動 Trigger Source。
    int captureChannel = 0;
};
