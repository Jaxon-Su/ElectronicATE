#pragma once

#include "icommunication.h"
#include <QString>
#include <QSerialPort>

class CommunicationFactory
{
public:
    // 解析 resource，回傳對應協議物件（用戶要記得 delete）
    static ICommunication* create(const QString& resource);

private:
    // 解析串口參數的輔助結構
    struct SerialPortParams {
        QString portName;              // COM3, /dev/ttyUSB0
        int baudRate;                  // 9600, 115200, etc.
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;
        QSerialPort::FlowControl flowControl;
        bool valid;                    // 解析是否成功
    };

    // 解析串口字串 (例如: "COM3:9600:8:N:1")
    static SerialPortParams parseSerialParams(const QString& resource);

    // 轉換字元到 Qt 列舉
    static QSerialPort::DataBits parseDataBits(const QString& str);
    static QSerialPort::Parity parseParity(const QString& str);
    static QSerialPort::StopBits parseStopBits(const QString& str);
    static QSerialPort::FlowControl parseFlowControl(const QString& str);
};
