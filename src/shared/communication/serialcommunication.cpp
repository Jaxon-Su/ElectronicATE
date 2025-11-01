// SerialCommunication.cpp
#include "serialcommunication.h"
#include <QDebug>

SerialCommunication::SerialCommunication(const QString& portName, int baudRate)
    : m_portName(portName), m_baudRate(baudRate)
{
    m_port = new QSerialPort();
}

SerialCommunication::~SerialCommunication() {
    if (m_port && m_port->isOpen())
        m_port->close();
    delete m_port;
}

bool SerialCommunication::open() {
    if (isOpen()) {
        m_error.clear();
        return true;
    }
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baudRate);
    if (!m_port->open(QIODevice::ReadWrite)) {
        m_error = QString("Serial port open failed: %1").arg(m_port->errorString());
        return false;
    }
    m_error.clear();
    return true;
}


void SerialCommunication::close() {
    if (m_port && m_port->isOpen()) {
        m_port->close();
        // 檢查是否真的關閉成功
        if (m_port->isOpen()) {
            m_error = QString("Serial port close failed: %1").arg(m_port->errorString());
            return;
        }
    }
    m_error.clear();
}



int SerialCommunication::write(const QByteArray& data) {
    if (!isOpen()) {
        m_error = "Serial port not open";
        return -1;
    }
    qint64 written = m_port->write(data);
    if (!m_port->waitForBytesWritten(3000)) {
        m_error = "Serial write timeout";
        return -1;
    }
    if (written < 0) {
        m_error = "Serial write failed";
        return -1;
    }
    m_error.clear();
    return static_cast<int>(written);
}

int SerialCommunication::read(QByteArray& data, int maxLen) {
    if (!isOpen()) {
        m_error = "Serial port not open";
        return -1;
    }
    if (!m_port->waitForReadyRead(3000)) {
        m_error = "Serial read timeout";
        return -1;
    }
    QByteArray buf = m_port->read(maxLen);
    if (buf.isEmpty()) {
        m_error = "Serial read failed or no data";
        return -1;
    }
    data = buf;
    m_error.clear();
    return buf.size();
}


bool SerialCommunication::isOpen() const {
    return m_port && m_port->isOpen();
}
