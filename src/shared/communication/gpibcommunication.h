#pragma once

#include "icommunication.h"
#include <visa.h>          // NI-VISA 標頭
#include <QString>

class GpibCommunication : public ICommunication
{
public:
    GpibCommunication(const QString& resource); // GPIB 資源名稱（如 "GPIB0::1::INSTR"）
    ~GpibCommunication() override;

    bool open() override;
    void close() override;
    int write(const QByteArray& data) override;
    int read(QByteArray& data, int maxLen) override;
    bool isOpen() const override;
    QString lastError() const override { return m_error; }

private:
    QString m_resource;      // GPIB 資源名稱
    ViSession m_rm = 0;      // VISA Resource Manager
    ViSession m_instr = 0;   // 儀器 Session
    bool m_opened = false;
    QString m_error;
};
