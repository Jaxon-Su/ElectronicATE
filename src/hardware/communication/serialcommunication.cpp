// SerialCommunication.cpp
#include "serialcommunication.h"
#include <QDebug>

SerialCommunication::SerialCommunication(const QString& portName,
                                         int baudRate,
                                         QSerialPort::DataBits dataBits,
                                         QSerialPort::Parity parity,
                                         QSerialPort::StopBits stopBits,
                                         QSerialPort::FlowControl flowControl)
    : m_portName(portName)
    , m_baudRate(baudRate)
    , m_dataBits(dataBits)
    , m_parity(parity)
    , m_stopBits(stopBits)
    , m_flowControl(flowControl)
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

    // 設定所有串口參數
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baudRate);
    m_port->setDataBits(m_dataBits);
    m_port->setParity(m_parity);
    m_port->setStopBits(m_stopBits);
    m_port->setFlowControl(m_flowControl);

    // 開啟串口
    if (!m_port->open(QIODevice::ReadWrite)) {
        m_error = QString("Serial port open failed: %1").arg(m_port->errorString());
        qWarning() << "[SerialCommunication]" << m_error;
        return false;
    }

    // 記錄成功開啟的參數
    qDebug() << "[SerialCommunication] Opened:" << m_portName
             << "Baud:" << m_baudRate
             << "Data:" << m_dataBits
             << "Parity:" << m_parity
             << "Stop:" << m_stopBits
             << "Flow:" << m_flowControl;

    m_error.clear();
    return true;
}

void SerialCommunication::close() {
    if (m_port && m_port->isOpen()) {
        m_port->close();
        if (m_port->isOpen()) {
            m_error = QString("Serial port close failed: %1").arg(m_port->errorString());
            return;
        }
        qDebug() << "[SerialCommunication] Closed:" << m_portName;
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
        qWarning() << "[SerialCommunication]" << m_error;
        return -1;
    }

    if (written < 0) {
        m_error = QString("Serial write failed: %1").arg(m_port->errorString());
        qWarning() << "[SerialCommunication]" << m_error;
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
        qWarning() << "[SerialCommunication]" << m_error;
        return -1;
    }

    QByteArray buf = m_port->read(maxLen);

    if (buf.isEmpty()) {
        m_error = "Serial read failed or no data";
        qWarning() << "[SerialCommunication]" << m_error;
        return -1;
    }

    data = buf;
    m_error.clear();
    return buf.size();
}

bool SerialCommunication::isOpen() const {
    return m_port && m_port->isOpen();
}
