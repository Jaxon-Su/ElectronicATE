#include "relay_rtu_4.h"

Relay_RTU_4::Relay_RTU_4(ICommunication* comm, quint8 slaveAddr) : RelayBase(comm), m_slaveAddr(slaveAddr)
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
    if (channel < 1 || channel > m_maxChannels)
        return false;
    bool ok = m_proto->writeSingleCoil(m_slaveAddr, static_cast<quint16>(channel - 1), true);
    return ok && readWriteEcho(0x05, channel - 1, 0xFF00);
}

bool Relay_RTU_4::turnOff(int channel)
{
    if (channel < 1 || channel > m_maxChannels)
        return false;
    bool ok = m_proto->writeSingleCoil(m_slaveAddr, static_cast<quint16>(channel - 1), false);
    return ok && readWriteEcho(0x05, channel - 1, 0x0000);
}

// FC 0F: 批次寫入全部 4 個繼電器狀態
// channels 列表中的通道設為 on，未在列表中的通道設為 !on
bool Relay_RTU_4::setMultiChannels(const QVector<int>& channels, bool on)
{
    quint8 bitmask = on ? 0x00 : 0x0F; // 預設：on→全部OFF，off→全部ON
    for (int ch : channels) {
        if (ch >= 1 && ch <= m_maxChannels) {
            if (on)
                bitmask |= static_cast<quint8>(1 << (ch - 1));
            else
                bitmask &= ~static_cast<quint8>(1 << (ch - 1));
        }
    }
    bool ok = m_proto->writeMultipleCoils(m_slaveAddr, 0x0000, static_cast<quint16>(m_maxChannels), bitmask);
    return ok && readWriteEcho(0x0F, 0, m_maxChannels);
}

bool Relay_RTU_4::readWriteEcho(quint8 function, quint16 address, quint16 value)
{
    QByteArray expected;
    for (quint8 byte :
         {m_slaveAddr, function, quint8(address >> 8), quint8(address), quint8(value >> 8), quint8(value)})
        expected.append(char(byte));
    const auto crc = Modbus_RTU_Protocol::calcCRC(expected);
    expected.append(char(crc));
    expected.append(char(crc >> 8));
    QByteArray echo;
    while (echo.size() < expected.size()) {
        QByteArray part;
        if (read(part, expected.size() - echo.size()) <= 0 || part.isEmpty())
            return false;
        echo += part;
        if (echo.size() >= 2 && quint8(echo[1]) != function)
            return false;
    }
    return echo == expected;
}

// FC 01: 讀取單一繼電器狀態
// 回應格式: [addr][01][byte_count=1][data][CRC_L][CRC_H] = 6 bytes
RelayStatus Relay_RTU_4::getStatus(int channel)
{
    if (channel < 1 || channel > m_maxChannels)
        return RelayStatus::Unknown;

    if (!m_proto->readRequest(m_slaveAddr, 0x01, static_cast<quint16>(channel - 1), 1))
        return RelayStatus::Unknown;

    QByteArray resp;
    while (resp.size() < 6) {
        QByteArray part;
        if (read(part, 6 - resp.size()) <= 0 || part.isEmpty())
            return RelayStatus::Unknown;
        resp += part;
        if (resp.size() >= 2 && (quint8(resp[0]) != m_slaveAddr || quint8(resp[1]) != 0x01))
            return RelayStatus::Unknown;
    }

    if (static_cast<quint8>(resp[0]) != m_slaveAddr || static_cast<quint8>(resp[1]) != 0x01 ||
        static_cast<quint8>(resp[2]) != 1)
        return RelayStatus::Unknown;

    // 驗證 CRC (CRC 計算前 4 bytes，frame 中以 little-endian 存放)
    quint16 recvCrc = static_cast<quint8>(resp[4]) | (static_cast<quint8>(resp[5]) << 8);
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
