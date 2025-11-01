// TcpCommunication.cpp
#include "tcpcommunication.h"
#include <QDebug>

TcpCommunication::TcpCommunication(const QString& ip, quint16 port)
    : m_ip(ip), m_port(port)
{
    m_socket = new QTcpSocket();
}

TcpCommunication::~TcpCommunication() {
    if (m_socket && m_socket->isOpen())
        m_socket->close();
    delete m_socket;
}

bool TcpCommunication::open() {
    if (isOpen()) {
        m_error.clear();
        return true;
    }
    m_socket->connectToHost(m_ip, m_port);
    if (!m_socket->waitForConnected(3000)) {
        m_error = QString("TCP connect failed: %1:%2 [%3]")
        .arg(m_ip).arg(m_port).arg(m_socket->errorString());
        return false;
    }
    m_error.clear();
    return true;
}


void TcpCommunication::close() {
    if (m_socket && m_socket->isOpen()) {
        m_socket->close();
        // 判斷是否真的關閉成功
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_error = QString("TCP close failed: state=%1, err=%2")
            .arg(m_socket->state())
                .arg(m_socket->errorString());
            return;
        }
    }
    m_error.clear();
}


int TcpCommunication::write(const QByteArray& data) {
    if (!isOpen()) {
        m_error = "TCP socket not open";
        return -1;
    }
    qint64 written = m_socket->write(data);
    if (!m_socket->waitForBytesWritten(3000)) {
        m_error = "TCP write timeout";
        return -1;
    }
    if (written < 0) {
        m_error = "TCP write failed";
        return -1;
    }
    m_error.clear();
    return static_cast<int>(written);
}

int TcpCommunication::read(QByteArray& data, int maxLen) {
    if (!isOpen()) {
        m_error = "TCP socket not open";
        return -1;
    }
    if (!m_socket->waitForReadyRead(3000)) {
        m_error = "TCP read timeout";
        return -1;
    }
    QByteArray buf = m_socket->read(maxLen);
    if (buf.isEmpty()) {
        m_error = "TCP read failed or no data";
        return -1;
    }
    data = buf;
    m_error.clear();
    return buf.size();
}


bool TcpCommunication::isOpen() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}
