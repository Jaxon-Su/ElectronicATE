#pragma once
#include <QByteArray>
#include <QVector>
#include "icommunication.h" // 使用您的介面

class Modbus_RTU_Protocol {
public:
    explicit Modbus_RTU_Protocol(ICommunication* comm) : m_comm(comm) {}

    // --- 寫入功能 ---

    // Function 05: Write Single Coil (用於控制 Relay On/Off)
    bool writeSingleCoil(quint8 slaveAddr, quint16 regAddr, bool state) {
        QByteArray frame;
        frame.append(static_cast<char>(slaveAddr));
        frame.append(0x05);
        frame.append(static_cast<char>(regAddr >> 8));
        frame.append(static_cast<char>(regAddr & 0xFF));
        frame.append(state ? static_cast<char>(0xFF) : 0x00);
        frame.append(static_cast<char>(0x00));
        return sendFrame(frame);
    }

    // Function 06: Write Single Holding Register
    bool writeSingleRegister(quint8 slaveAddr, quint16 regAddr, quint16 value) {
        QByteArray frame;
        frame.append(static_cast<char>(slaveAddr));
        frame.append(0x06);
        frame.append(static_cast<char>(regAddr >> 8));
        frame.append(static_cast<char>(regAddr & 0xFF));
        frame.append(static_cast<char>(value >> 8));
        frame.append(static_cast<char>(value & 0xFF));
        return sendFrame(frame);
    }

    // Function 0F: Write Multiple Coils
    // bitmask: bit0 = coil 0, bit1 = coil 1, ...
    bool writeMultipleCoils(quint8 slaveAddr, quint16 startAddr, quint16 count, quint8 bitmask) {
        QByteArray frame;
        frame.append(static_cast<char>(slaveAddr));
        frame.append(static_cast<char>(0x0F));
        frame.append(static_cast<char>(startAddr >> 8));
        frame.append(static_cast<char>(startAddr & 0xFF));
        frame.append(static_cast<char>(count >> 8));
        frame.append(static_cast<char>(count & 0xFF));
        frame.append(static_cast<char>(0x01));   // byte count = 1
        frame.append(static_cast<char>(bitmask));
        return sendFrame(frame);
    }

    // --- 讀取功能 ---

    // Function 01/02/03/04 通用讀取請求
    bool readRequest(quint8 slaveAddr, quint8 funcCode, quint16 startAddr, quint16 count) {
        QByteArray frame;
        frame.append(static_cast<char>(slaveAddr));
        frame.append(static_cast<char>(funcCode));
        frame.append(static_cast<char>(startAddr >> 8));
        frame.append(static_cast<char>(startAddr & 0xFF));
        frame.append(static_cast<char>(count >> 8));
        frame.append(static_cast<char>(count & 0xFF));
        return sendFrame(frame);
    }

    static quint16 calcCRC(const QByteArray& data) {
        quint16 crc = 0xFFFF;
        for (int i = 0; i < data.size(); ++i) {
            crc ^= static_cast<quint8>(data[i]);
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
                else crc >>= 1;
            }
        }
        return crc;
    }

private:
    ICommunication* m_comm;

    bool sendFrame(QByteArray& frame) {
        if (!m_comm || !m_comm->isOpen()) return false;
        appendCRC(frame);
        return m_comm->write(frame) > 0;
    }

    void appendCRC(QByteArray& frame) {
        quint16 crc = calcCRC(frame);
        frame.append(static_cast<char>(crc & 0xFF));
        frame.append(static_cast<char>((crc >> 8) & 0xFF));
    }
};
