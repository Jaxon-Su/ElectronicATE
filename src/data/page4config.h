#pragma once

#include <QString>
#include <QDateTime>

// 連線狀態枚舉
enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// 通訊類型枚舉
enum class CommType {
    TCPIP,
    GPIB,
    Serial,
    USB,
    Modbus,
    Unknown
};

// 命令歷史記錄結構
struct CommandHistory {
    QString timestamp;
    QString address;
    QString command;
    QString response;
    bool success = false;
};
