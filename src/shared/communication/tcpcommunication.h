// TcpCommunication.h
#pragma once
#include "icommunication.h"
#include <QTcpSocket>
#include <QString>

class TcpCommunication : public ICommunication {
public:
    TcpCommunication(const QString& ip, quint16 port);
    ~TcpCommunication() override;

    bool open() override;
    void close() override;
    int write(const QByteArray& data) override;
    int read(QByteArray& data, int maxLen) override;
    bool isOpen() const override;
    QString lastError() const override { return m_error; }

private:
    QString m_ip;
    quint16 m_port;
    QTcpSocket* m_socket = nullptr;
    QString m_error;
};
