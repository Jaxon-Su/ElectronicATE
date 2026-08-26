#pragma once

#include "relay.h"
#include "modbusrtuprotocol.h"

class Relay_RTU_4 : public RelayBase {
public:
    explicit Relay_RTU_4(ICommunication* comm, quint8 slaveAddr = 0x01);
    virtual ~Relay_RTU_4();

    bool turnOn(int channel) override;
    bool turnOff(int channel) override;
    bool setMultiChannels(const QVector<int>& channels, bool on) override; // 檢查 const 與 &
    RelayStatus getStatus(int channel) override;

    QString model() const override;
    QString vendor() const override;

private:
    Modbus_RTU_Protocol* m_proto;
    quint8 m_slaveAddr;
};
