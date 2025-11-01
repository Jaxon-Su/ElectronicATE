// SerialCommunication.h
#pragma once
#include "icommunication.h"
#include <QSerialPort>
#include <QString>

class SerialCommunication : public ICommunication {
public:
    SerialCommunication(const QString& portName, int baudRate = 9600);
    ~SerialCommunication() override;

    bool open() override;
    void close() override;
    int write(const QByteArray& data) override;
    int read(QByteArray& data, int maxLen) override;
    bool isOpen() const override;
    QString lastError() const override { return m_error; }

private:
    QString m_portName;
    int m_baudRate;
    QSerialPort* m_port = nullptr;
    QString m_error;
};
