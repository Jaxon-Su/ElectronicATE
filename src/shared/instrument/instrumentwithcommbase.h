#pragma once
#include "instrumentbase.h"
#include "iinstrumentcomm.h"
#include "icommunication.h"

class InstrumentWithCommBase : public InstrumentBase, public IInstrumentComm
{
public:
    InstrumentWithCommBase(ICommunication* comm = nullptr)
        : m_comm(comm), m_connected(false), m_address("") {}

    virtual ~InstrumentWithCommBase() {
        if (m_connected && m_comm) {
            disconnect();
        }
    }

    void connect() override;
    void disconnect() override;
    bool isConnected() const override;
    void setAddress(const QString& addr) override;
    QString getaddress() const override;

    void setCommunication(ICommunication* comm);
    QString lastError() const { return m_lastError; }

protected:

    QString m_address;
    bool    m_connected;
    ICommunication* m_comm;

    int write(const QString& s);
    int write(const QByteArray& data);

    int read(QByteArray& data, int maxLen);

    QString m_lastError;
    bool queryInt(const QString& cmd, int& value);
    bool queryDouble(const QString& cmd, double& value);
    bool queryString(const QString& cmd, QString& result);
    bool queryBinary(const QString& cmd, QByteArray& out,
                     int maxHeaderBytes = 32);

    void sendCommandWithLog(const QString& cmd, const QString& tag);

};

