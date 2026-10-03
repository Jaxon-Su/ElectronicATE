#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QScopeGuard>
#include "transfercancellation.h"

class ICommunication
{
  public:
    virtual ~ICommunication() {}
    virtual bool open() = 0;                            // 打開連線
    virtual void close() = 0;                           // 關閉連線
    virtual int write(const QByteArray &data) = 0;      // 寫入資料
    virtual int read(QByteArray &data, int maxLen) = 0; // 讀取資料
    virtual bool isOpen() const = 0;                    // 狀態查詢
    virtual QString lastError() const = 0;

    // 動態調整 I/O 逾時（毫秒）。
    // 大檔傳輸前呼叫拉長，傳輸後還原。非 VISA 實作可忽略（no-op）。
    virtual void setTimeoutMs(int /*ms*/) {}
    virtual int timeoutMs() const { return 10000; }

    // VXI-11 Device Clear：清除儀器的 input queue 與 output buffer，
    // 確保前次失敗留下的殘留資料不會污染下一次查詢。
    // 非 VISA 實作可忽略（no-op）。
    virtual bool deviceClear() { return true; }

    // read() returns zero only at a clean end; negative values are errors/timeouts.
    // Message-oriented transports override this to recognize their framing/END marker.
    virtual bool readFully(QByteArray &data)
    {
        constexpr int chunkSize = 64 * 1024;
        data.clear();
        const int timeout = timeoutMs();
        QElapsedTimer elapsed;
        elapsed.start();
        const auto restore = qScopeGuard([&] { setTimeoutMs(timeout); });
        for (;;) {
            const auto remaining = timeout - elapsed.elapsed();
            if (remaining <= 0 || TransferCancellation::requested()) {
                data.clear();
                return false;
            }
            setTimeoutMs(static_cast<int>(remaining));
            QByteArray chunk;
            const int bytes = read(chunk, chunkSize);
            if (TransferCancellation::requested() || elapsed.elapsed() >= timeout ||
                bytes < 0 || bytes > chunk.size() || bytes > chunkSize) {
                data.clear();
                return false;
            }
            if (bytes == 0)
                return !data.isEmpty();
            data.append(chunk.constData(), bytes);
        }
    }
};
