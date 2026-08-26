// SerialCommunication.h
#pragma once
#include "icommunication.h"
#include <QSerialPort>
#include <QString>

class SerialCommunication : public ICommunication {
public:
    SerialCommunication(const QString& portName,
                        int baudRate = 9600,
                        QSerialPort::DataBits dataBits = QSerialPort::Data8,
                        QSerialPort::Parity parity = QSerialPort::NoParity,
                        QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl);

    ~SerialCommunication() override;

    bool open() override;
    void close() override;
    int write(const QByteArray& data) override;
    int read(QByteArray& data, int maxLen) override;
    bool isOpen() const override;
    QString lastError() const override { return m_error; }

    void setDataBits(QSerialPort::DataBits dataBits) { m_dataBits = dataBits; }
    void setParity(QSerialPort::Parity parity) { m_parity = parity; }
    void setStopBits(QSerialPort::StopBits stopBits) { m_stopBits = stopBits; }
    void setFlowControl(QSerialPort::FlowControl flowControl) { m_flowControl = flowControl; }

private:
    QString m_portName;
    int m_baudRate;
    QSerialPort::DataBits m_dataBits;
    QSerialPort::Parity m_parity;
    QSerialPort::StopBits m_stopBits;
    QSerialPort::FlowControl m_flowControl;
    QSerialPort* m_port = nullptr;
    QString m_error;
};
