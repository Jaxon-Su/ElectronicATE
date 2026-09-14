#include "relay_rtu_4.h"

Relay_RTU_4::Relay_RTU_4(ICommunication* comm, quint8 slaveAddr)
    : RelayBase(comm), m_slaveAddr(slaveAddr)
{
    m_proto = new Modbus_RTU_Protocol(comm);
    setMaxChannels(4);
}

Relay_RTU_4::~Relay_RTU_4()
{
    if (m_proto) {
        delete m_proto;
        m_proto = nullptr;
    }
}

// FC 05: 控制單一繼電器 ON/OFF，並讀回設備的 echo 回應 (8 bytes)
bool Relay_RTU_4::turnOn(int channel)
{
    if (channel < 1 || channel > m_maxChannels) return false;
    bool ok = m_proto->writeSingleCoil(m_slaveAddr,
                                       static_cast<quint16>(channel - 1),
                                       true);
    if (ok) {
        QByteArray echo;
        read(echo, 8); // FC05 echo: [addr][05][addr_H][addr_L][FF][00][CRC_L][CRC_H]
    }
    return ok;
}

bool Relay_RTU_4::turnOff(int channel)
{
    if (channel < 1 || channel > m_maxChannels) return false;
    bool ok = m_proto->writeSingleCoil(m_slaveAddr,
                                       static_cast<quint16>(channel - 1),
                                       false);
    if (ok) {
        QByteArray echo;
        read(echo, 8); // FC05 echo: [addr][05][addr_H][addr_L][00][00][CRC_L][CRC_H]
    }
    return ok;
}

// FC 0F: 批次寫入全部 4 個繼電器狀態
// channels 列表中的通道設為 on，未在列表中的通道設為 !on
bool Relay_RTU_4::setMultiChannels(const QVector<int>& channels, bool on)
{
    quint8 bitmask = on ? 0x00 : 0x0F; // 預設：on→全部OFF，off→全部ON
    for (int ch : channels) {
        if (ch >= 1 && ch <= m_maxChannels) {
            if (on) bitmask |=  static_cast<quint8>(1 << (ch - 1));
            else    bitmask &= ~static_cast<quint8>(1 << (ch - 1));
        }
    }
    bool ok = m_proto->writeMultipleCoils(m_slaveAddr,
                                          0x0000,
                                          static_cast<quint16>(m_maxChannels),
                                          bitmask);
    if (ok) {
        QByteArray echo;
        read(echo, 8); // FC0F echo: [addr][0F][start_H][start_L][count_H][count_L][CRC_L][CRC_H]
    }
    return ok;
}

// FC 01: 讀取單一繼電器狀態
// 回應格式: [addr][01][byte_count=1][data][CRC_L][CRC_H] = 6 bytes
RelayStatus Relay_RTU_4::getStatus(int channel)
{
    if (channel < 1 || channel > m_maxChannels) return RelayStatus::Unknown;

    if (!m_proto->readRequest(m_slaveAddr, 0x01,
                              static_cast<quint16>(channel - 1), 1))
        return RelayStatus::Unknown;

    QByteArray resp;
    if (read(resp, 6) < 6 || resp.size() < 6)
        return RelayStatus::Unknown;

    if (static_cast<quint8>(resp[0]) != m_slaveAddr ||
        static_cast<quint8>(resp[1]) != 0x01 || static_cast<quint8>(resp[2]) != 1)
        return RelayStatus::Unknown;

    // 驗證 CRC (CRC 計算前 4 bytes，frame 中以 little-endian 存放)
    quint16 recvCrc = static_cast<quint8>(resp[4]) |
                      (static_cast<quint8>(resp[5]) << 8);
    if (Modbus_RTU_Protocol::calcCRC(resp.left(4)) != recvCrc)
        return RelayStatus::Unknown;

    // data byte: Bit0 = 指定 coil 狀態 (1=Closed, 0=Open)
    return (resp[3] & 0x01) ? RelayStatus::Closed : RelayStatus::Open;
}

QString Relay_RTU_4::model() const
{
    return "Modbus_RTU_4CH_Relay";
}

QString Relay_RTU_4::vendor() const
{
    return "Waveshare";
}
