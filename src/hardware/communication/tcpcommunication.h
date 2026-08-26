#pragma once

#include "icommunication.h"
#include <visa.h>
#include <QString>

class TcpCommunication : public ICommunication {
public:
    // 資源名稱範例: "TCPIP::192.168.1.10::5025::SOCKET" 或 "TCPIP0::192.168.1.10::INSTR"
    // timeoutMs: VISA 讀寫逾時（毫秒），預設 10000ms，大檔傳輸建議設更長
    explicit TcpCommunication(const QString& resource, int timeoutMs = 10000);
    ~TcpCommunication() override;

    bool open() override;
    void close() override;
    int  write(const QByteArray& data) override;
    int  read(QByteArray& data, int maxLen) override;
    bool isOpen() const override;
    QString lastError() const override { return m_error; }

    // VXI-11 最佳化：利用 VI_SUCCESS END 指示器立即停止，不等 timeout
    bool readFully(QByteArray& data) override;

    // 動態調整 VISA I/O 逾時（連線前後皆可呼叫）
    // 小指令用預設值即可；大檔傳輸（如 CSV READFile）前請先呼叫此函數拉長 timeout
    void setTimeoutMs(int ms) override;
    int  timeoutMs() const { return m_timeoutMs; }

    // 送出 VXI-11 Device Clear，清除儀器 input/output buffer 殘留資料
    bool deviceClear() override;

private:
    QString    m_resource;          // VISA 資源名稱
    ViSession  m_rm    = 0;         // VISA Resource Manager
    ViSession  m_instr = 0;         // 儀器 Session
    bool       m_opened = false;
    int        m_timeoutMs;         // 目前的逾時設定（毫秒）
    QString    m_error;
};
