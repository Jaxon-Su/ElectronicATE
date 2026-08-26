// src/shared/communication/ICommunication.h
#pragma once

#include <QByteArray>

class ICommunication {
public:
    virtual ~ICommunication() {}
    virtual bool open() = 0;                             // 打開連線
    virtual void close() = 0;                            // 關閉連線
    virtual int write(const QByteArray& data) = 0;       // 寫入資料
    virtual int read(QByteArray& data, int maxLen) = 0;  // 讀取資料
    virtual bool isOpen() const = 0;                     // 狀態查詢
    virtual QString lastError() const = 0;

    // 動態調整 I/O 逾時（毫秒）。
    // 大檔傳輸前呼叫拉長，傳輸後還原。非 VISA 實作可忽略（no-op）。
    virtual void setTimeoutMs(int /*ms*/) {}

    // VXI-11 Device Clear：清除儀器的 input queue 與 output buffer，
    // 確保前次失敗留下的殘留資料不會污染下一次查詢。
    // 非 VISA 實作可忽略（no-op）。
    virtual bool deviceClear() { return true; }

    // 讀取完整回應直到傳輸結束。
    // 預設實作：持續 read() 直到 timeout（bytes <= 0）。
    // VXI-11 實作應 override，利用 VI_SUCCESS END 指示器立即停止，避免等待 timeout。
    virtual bool readFully(QByteArray& data) {
        static const int CHUNK_SIZE = 64 * 1024;
        data.clear();
        QByteArray chunk;
        int bytes;
        while ((bytes = read(chunk, CHUNK_SIZE)) > 0)
            data.append(chunk.constData(), bytes);
        return !data.isEmpty();
    }
};
