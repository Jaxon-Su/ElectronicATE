#pragma once

#include "icommunication.h"
#include <visa.h>
#include <QString>

class GpibCommunication : public ICommunication
{
public:
    // resource  : GPIB 資源名稱（如 "GPIB0::1::INSTR"）
    // timeoutMs : VISA I/O 逾時（毫秒），預設 10000ms
    explicit GpibCommunication(const QString& resource, int timeoutMs = 10000);
    ~GpibCommunication() override;

    bool open()  override;
    void close() override;
    int  write(const QByteArray& data)        override;
    int  read(QByteArray& data, int maxLen)   override;
    bool isOpen()    const override;
    QString lastError() const override { return m_error; }

    // ── 與 TcpCommunication 對齊的擴充方法 ──────────────────────────────────

    // 利用 VI_SUCCESS EOI 指示器立即停止，不等 timeout
    bool readFully(QByteArray& data) override;

    // 動態調整 VISA I/O 逾時（連線前後皆可呼叫）
    void setTimeoutMs(int ms) override;
    int  timeoutMs() const { return m_timeoutMs; }

    // viClear（清儀器端 buffer）+ viFlush（清 VISA 軟體層 buffer）
    bool deviceClear() override;

private:
    QString    m_resource;
    ViSession  m_rm    = 0;
    ViSession  m_instr = 0;
    bool       m_opened = false;
    int        m_timeoutMs;
    QString    m_error;
};
