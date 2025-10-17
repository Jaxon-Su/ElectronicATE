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
};
