#pragma once
#include <memory>
#include <atomic>
#include <functional>
#include <QString>

class Oscilloscope;

// 擷取命令執行所需的共享上下文
// 由 ViewModel 組裝後傳入各 CaptureCommand，解耦命令與 ViewModel
struct CaptureContext {
    // 非同步擷取期間保持 oscilloscope 存活（shared_ptr 防 use-after-free）
    std::shared_ptr<Oscilloscope> oscilloscope;

    // 跨執行緒的互斥旗標，防止同時執行多個擷取
    std::shared_ptr<std::atomic<bool>> captureInProgress;

    // 預設儲存目錄（上次使用的路徑）
    QString lastSaveDir;

    // 檔案對話框確認後，通知 ViewModel 更新並持久化路徑
    // 在 execute() 內同步呼叫（file dialog 之後、async 之前）
    std::function<void(const QString&)> onSaveDirChanged;

    // 使用者在 Trigger Source ComboBox 選擇的通道（1-based）。
    // 由 buildCaptureContext() 直接讀 UI，不查詢儀器，確保與顯示一致。
    // 0 = 未指定（fallback 到 trigger source 查詢）
    int captureChannel = 0;
};
