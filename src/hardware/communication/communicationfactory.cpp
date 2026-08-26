#include "communicationfactory.h"
#include "gpibcommunication.h"
#include "tcpcommunication.h"
#include "serialcommunication.h"
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QDebug>

ICommunication* CommunicationFactory::create(const QString& resource)
{
    if (resource.isEmpty()) {
        qWarning() << "[CommunicationFactory] Empty resource string";
        return nullptr;
    }

    // ===== GPIB 處理 =====
    // 純數字也當作 GPIB
    static const QRegularExpression numberOnly("^[0-9]+$");
    if (numberOnly.match(resource).hasMatch()) {
        qDebug() << "[CommunicationFactory] Creating GPIB from number:" << resource;
        return new GpibCommunication("GPIB0::" + resource + "::INSTR");
    }

    if (resource.startsWith("GPIB", Qt::CaseInsensitive)) {
        qDebug() << "[CommunicationFactory] Creating GPIB:" << resource;
        return new GpibCommunication(resource);
    }

    // ===== Modbus RTU 處理 =====
    // 格式: MODBUS::COM3:9600:8:N:1::SLAVE:1
    if (resource.startsWith("MODBUS", Qt::CaseInsensitive)) {
        qDebug() << "[CommunicationFactory] Modbus resource detected:" << resource;

        // 解析 Slave ID
        static const QRegularExpression slaveRx("SLAVE:(\\d+)",
                                                QRegularExpression::CaseInsensitiveOption);
        auto slaveMatch = slaveRx.match(resource);
        int slaveId = slaveMatch.hasMatch() ? slaveMatch.captured(1).toInt() : 1;

        // 判斷是 RTU 還是 TCP
        if (resource.contains("COM", Qt::CaseInsensitive) ||
            resource.contains("/dev/tty", Qt::CaseInsensitive)) {
            // Modbus RTU - 基於串口
            // 提取串口部分
            static const QRegularExpression rtuRx(
                "MODBUS::([^:]+):(\\d+):(\\d+):([NEOSM]):(\\d\\.?\\d?)",
                QRegularExpression::CaseInsensitiveOption);
            auto match = rtuRx.match(resource);

            if (match.hasMatch()) {
                QString portName = match.captured(1);
                int baudRate = match.captured(2).toInt();
                int dataBits = match.captured(3).toInt();
                QString parity = match.captured(4);
                QString stopBits = match.captured(5);

                qDebug() << "[CommunicationFactory] Modbus RTU config:"
                         << "Port:" << portName
                         << "Baud:" << baudRate
                         << "SlaveId:" << slaveId;

                // 使用串口通訊作為底層（如果沒有專門的 Modbus 類別）
                // 如果有 ModbusRtuCommunication 類別，改用：
                // return new ModbusRtuCommunication(portName, baudRate, slaveId, ...);

                return new SerialCommunication(
                    portName,
                    baudRate,
                    parseDataBits(QString::number(dataBits)),
                    parseParity(parity),
                    parseStopBits(stopBits),
                    QSerialPort::NoFlowControl
                    );
            }
        } else {
            // Modbus TCP
            // 格式: MODBUS::192.168.1.100:502::SLAVE:1
            static const QRegularExpression tcpRx(
                "MODBUS::([\\d\\.]+):(\\d+)",
                QRegularExpression::CaseInsensitiveOption);
            auto match = tcpRx.match(resource);

            if (match.hasMatch()) {
                QString ip = match.captured(1);
                int port = match.captured(2).toInt();

                qDebug() << "[CommunicationFactory] Modbus TCP config:"
                         << "IP:" << ip
                         << "Port:" << port
                         << "SlaveId:" << slaveId;

                // 使用 TCP 通訊作為底層
                // 如果有 ModbusTcpCommunication 類別，改用：
                // return new ModbusTcpCommunication(ip, port, slaveId);

                QString tcpResource = QString("TCPIP0::%1::%2::SOCKET").arg(ip).arg(port);
                return new TcpCommunication(tcpResource);
            }
        }

        qWarning() << "[CommunicationFactory] Failed to parse Modbus resource:" << resource;
        return nullptr;
    }

    // ===== TCP/IP 處理 =====
    if (resource.startsWith("TCPIP", Qt::CaseInsensitive)) {
        qDebug() << "[CommunicationFactory] Creating TCP:" << resource;
        return new TcpCommunication(resource);
    }

    // ===== 簡化的 TCP 格式處理 (IP:Port) =====
    // 保留給既有設定相容；示波器建議使用 VXI-11: TCPIP0::IP::INSTR
    static const QRegularExpression simpleIpPortRx("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)$");
    auto ipPortMatch = simpleIpPortRx.match(resource);
    if (ipPortMatch.hasMatch()) {
        QString ip = ipPortMatch.captured(1);
        int port = ipPortMatch.captured(2).toInt();
        QString tcpResource = QString("TCPIP0::%1::%2::SOCKET").arg(ip).arg(port);
        qDebug() << "[CommunicationFactory] Creating TCP from IP:Port:" << tcpResource;
        return new TcpCommunication(tcpResource);
    }

    // ===== USB 處理 =====
    if (resource.startsWith("USB", Qt::CaseInsensitive)) {
        qDebug() << "[CommunicationFactory] USB resource detected:" << resource;
        // USB 通常也走 VISA，可以嘗試使用 GPIB 類別（如果底層 VISA 支援）
        // 或者實作專門的 UsbCommunication 類別
        // return new UsbCommunication(resource);
        qWarning() << "[CommunicationFactory] USB not fully implemented, attempting VISA...";
        return new GpibCommunication(resource);  // 嘗試使用 VISA
    }

    // ===== Serial Port 處理 =====
    // 檢查是否為串口 (COM / /dev/tty)
    if (resource.startsWith("COM", Qt::CaseInsensitive) ||
        resource.startsWith("/dev/tty", Qt::CaseInsensitive))
    {
        // 解析串口參數
        SerialPortParams params = parseSerialParams(resource);

        if (!params.valid) {
            qWarning() << "[CommunicationFactory] Failed to parse serial params:" << resource;
            return nullptr;
        }

        qDebug() << "[CommunicationFactory] Serial port config:"
                 << "Port:" << params.portName
                 << "Baud:" << params.baudRate
                 << "Data:" << params.dataBits
                 << "Parity:" << params.parity
                 << "Stop:" << params.stopBits;

        return new SerialCommunication(
            params.portName,
            params.baudRate,
            params.dataBits,
            params.parity,
            params.stopBits,
            params.flowControl
            );
    }

    qWarning() << "[CommunicationFactory] Unknown resource format:" << resource;
    return nullptr;
}

// ========== 私有輔助函數 ==========

CommunicationFactory::SerialPortParams
CommunicationFactory::parseSerialParams(const QString& resource)
{
    SerialPortParams params;
    params.valid = false;

    // 分割字串 "COM3:9600:8:N:1" → ["COM3", "9600", "8", "N", "1"]
    QStringList parts = resource.split(':');

    if (parts.isEmpty()) {
        return params;
    }

    // 第 0 部分：Port Name（必須）
    params.portName = parts[0].trimmed();

    // 第 1 部分：Baud Rate（可選，預設 9600）
    if (parts.size() > 1 && !parts[1].isEmpty()) {
        bool ok;
        params.baudRate = parts[1].toInt(&ok);
        if (!ok || params.baudRate <= 0) {
            qWarning() << "[CommunicationFactory] Invalid baud rate:" << parts[1];
            params.baudRate = 9600;
        }
    } else {
        params.baudRate = 9600;
    }

    // 第 2 部分：Data Bits（可選，預設 8）
    if (parts.size() > 2 && !parts[2].isEmpty()) {
        params.dataBits = parseDataBits(parts[2]);
    } else {
        params.dataBits = QSerialPort::Data8;
    }

    // 第 3 部分：Parity（可選，預設 None）
    if (parts.size() > 3 && !parts[3].isEmpty()) {
        params.parity = parseParity(parts[3]);
    } else {
        params.parity = QSerialPort::NoParity;
    }

    // 第 4 部分：Stop Bits（可選，預設 1）
    if (parts.size() > 4 && !parts[4].isEmpty()) {
        params.stopBits = parseStopBits(parts[4]);
    } else {
        params.stopBits = QSerialPort::OneStop;
    }

    // 第 5 部分：Flow Control（可選，預設 None）
    if (parts.size() > 5 && !parts[5].isEmpty()) {
        params.flowControl = parseFlowControl(parts[5]);
    } else {
        params.flowControl = QSerialPort::NoFlowControl;
    }

    params.valid = true;
    return params;
}

QSerialPort::DataBits CommunicationFactory::parseDataBits(const QString& str)
{
    int bits = str.toInt();
    switch (bits) {
    case 5: return QSerialPort::Data5;
    case 6: return QSerialPort::Data6;
    case 7: return QSerialPort::Data7;
    case 8: return QSerialPort::Data8;
    default:
        qWarning() << "[CommunicationFactory] Invalid data bits:" << str << "- using 8";
        return QSerialPort::Data8;
    }
}

QSerialPort::Parity CommunicationFactory::parseParity(const QString& str)
{
    QString upper = str.toUpper().trimmed();

    if (upper == "N" || upper == "NONE" || upper == "NO") {
        return QSerialPort::NoParity;
    }
    else if (upper == "E" || upper == "EVEN") {
        return QSerialPort::EvenParity;
    }
    else if (upper == "O" || upper == "ODD") {
        return QSerialPort::OddParity;
    }
    else if (upper == "S" || upper == "SPACE") {
        return QSerialPort::SpaceParity;
    }
    else if (upper == "M" || upper == "MARK") {
        return QSerialPort::MarkParity;
    }
    else {
        qWarning() << "[CommunicationFactory] Invalid parity:" << str << "- using None";
        return QSerialPort::NoParity;
    }
}

QSerialPort::StopBits CommunicationFactory::parseStopBits(const QString& str)
{
    QString trimmed = str.trimmed();

    if (trimmed == "1" || trimmed == "ONE") {
        return QSerialPort::OneStop;
    }
    else if (trimmed == "1.5" || trimmed == "ONEANDHALF") {
        return QSerialPort::OneAndHalfStop;
    }
    else if (trimmed == "2" || trimmed == "TWO") {
        return QSerialPort::TwoStop;
    }
    else {
        qWarning() << "[CommunicationFactory] Invalid stop bits:" << str << "- using 1";
        return QSerialPort::OneStop;
    }
}

QSerialPort::FlowControl CommunicationFactory::parseFlowControl(const QString& str)
{
    QString upper = str.toUpper().trimmed();

    if (upper == "NONE" || upper == "NO" || upper == "N" || upper.isEmpty()) {
        return QSerialPort::NoFlowControl;
    }
    else if (upper == "HARDWARE" || upper == "HW" || upper == "RTS" || upper == "RTSCTS") {
        return QSerialPort::HardwareControl;
    }
    else if (upper == "SOFTWARE" || upper == "SW" || upper == "XON" || upper == "XONXOFF") {
        return QSerialPort::SoftwareControl;
    }
    else {
        qWarning() << "[CommunicationFactory] Invalid flow control:" << str << "- using None";
        return QSerialPort::NoFlowControl;
    }
}
