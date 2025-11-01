#pragma once
#include <QString>

class IInstrumentComm {
public:
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual void setAddress(const QString&) = 0;
    virtual QString getaddress() const = 0;
};
