#pragma once

#include <QString>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QStringList>
#include <QRegularExpression>

// ========== 協議類型枚舉 ==========
enum class ProtocolType {
    Unknown = 0,
    GPIB,
    TCP_VXI11,      // TCPIP::IP::INSTR
    TCP_Socket,     // TCPIP::IP::port::SOCKET
    TCP_HiSLIP,     // TCPIP::IP::hislip0::INSTR
    Serial,         // COM / RS-232 / RS-485
    USB,
    Modbus_RTU,     // Serial + Modbus RTU
    Modbus_TCP      // TCP + Modbus TCP
};

// ========== 協議類型工具函數 ==========
inline QString protocolTypeToString(ProtocolType type) {
    switch (type) {
    case ProtocolType::GPIB:        return "GPIB";
    case ProtocolType::TCP_VXI11:   return "TCP_VXI11";
    case ProtocolType::TCP_Socket:  return "TCP_Socket";
    case ProtocolType::TCP_HiSLIP:  return "TCP_HiSLIP";
    case ProtocolType::Serial:      return "Serial";
    case ProtocolType::USB:         return "USB";
    case ProtocolType::Modbus_RTU:  return "Modbus_RTU";
    case ProtocolType::Modbus_TCP:  return "Modbus_TCP";
    default:                        return "Unknown";
    }
}

inline ProtocolType stringToProtocolType(const QString& str) {
    if (str == "GPIB")        return ProtocolType::GPIB;
    if (str == "TCP_VXI11")   return ProtocolType::TCP_VXI11;
    if (str == "TCP_Socket")  return ProtocolType::TCP_Socket;
    if (str == "TCP_HiSLIP")  return ProtocolType::TCP_HiSLIP;
    if (str == "Serial")      return ProtocolType::Serial;
    if (str == "USB")         return ProtocolType::USB;
    if (str == "Modbus_RTU")  return ProtocolType::Modbus_RTU;
    if (str == "Modbus_TCP")  return ProtocolType::Modbus_TCP;
    return ProtocolType::Unknown;
}

// ========== 通訊配置結構 ==========
struct CommunicationConfig {
    ProtocolType protocol = ProtocolType::Unknown;

    // 通用參數
    int timeout = 5000;  // ms

    // --- GPIB 參數 ---
    int gpibBoard = 0;
    int gpibAddress = 1;
    int gpibSecondary = 0;

    // --- TCP/IP 參數 ---
    QString ipAddress;
    int port = 5025;

    // --- Serial 參數 ---
    QString portName;              // COM3, /dev/ttyUSB0
    int baudRate = 9600;
    int dataBits = 8;              // 5, 6, 7, 8
    QString parity = "N";          // N, E, O, S, M
    QString stopBits = "1";        // 1, 1.5, 2
    QString flowControl = "N";     // N, HW, SW

    // --- Modbus 特有參數 ---
    int slaveId = 1;
    int registerAddress = 0;
    QString modbusFunction = "03"; // 功能碼

    // --- USB 參數 ---
    QString vendorId;
    QString productId;
    QString serialNumber;

    // ========== 工具方法 ==========

    // 產生 VISA/連線字串
    QString toResourceString() const {
        switch (protocol) {
        case ProtocolType::GPIB:
            if (gpibSecondary > 0)
                return QString("GPIB%1::%2::%3::INSTR")
                    .arg(gpibBoard).arg(gpibAddress).arg(gpibSecondary);
            return QString("GPIB%1::%2::INSTR").arg(gpibBoard).arg(gpibAddress);

        case ProtocolType::TCP_VXI11:
            return QString("TCPIP0::%1::INSTR").arg(ipAddress);

        case ProtocolType::TCP_Socket:
            return QString("TCPIP0::%1::%2::SOCKET").arg(ipAddress).arg(port);

        case ProtocolType::TCP_HiSLIP:
            return QString("TCPIP0::%1::hislip0::INSTR").arg(ipAddress);

        case ProtocolType::Serial:
            return QString("%1:%2:%3:%4:%5:%6")
                .arg(portName)
                .arg(baudRate)
                .arg(dataBits)
                .arg(parity)
                .arg(stopBits)
                .arg(flowControl);

        case ProtocolType::Modbus_RTU:
            return QString("MODBUS::%1:%2:%3:%4:%5::SLAVE:%6")
                .arg(portName).arg(baudRate).arg(dataBits)
                .arg(parity).arg(stopBits).arg(slaveId);

        case ProtocolType::Modbus_TCP:
            return QString("MODBUS::%1:%2::SLAVE:%3")
                .arg(ipAddress).arg(port).arg(slaveId);

        case ProtocolType::USB:
            return QString("USB0::0x%1::0x%2::%3::INSTR")
                .arg(vendorId).arg(productId).arg(serialNumber);

        default:
            return QString();
        }
    }

    // 產生簡短顯示字串（用於UI）
    QString toDisplayString() const {
        switch (protocol) {
        case ProtocolType::GPIB:
            return QString("GPIB::%1").arg(gpibAddress);
        case ProtocolType::TCP_VXI11:
        case ProtocolType::TCP_Socket:
        case ProtocolType::TCP_HiSLIP:
            return QString("%1:%2").arg(ipAddress).arg(port);
        case ProtocolType::Serial:
            return QString("%1 %2bps").arg(portName).arg(baudRate);
        case ProtocolType::Modbus_RTU:
            return QString("%1 ID:%2").arg(portName).arg(slaveId);
        case ProtocolType::Modbus_TCP:
            return QString("%1:%2 ID:%3").arg(ipAddress).arg(port).arg(slaveId);
        case ProtocolType::USB:
            return QString("USB::%1").arg(serialNumber);
        default:
            return QString();
        }
    }

    // 檢查是否有效配置
    bool isValid() const {
        switch (protocol) {
        case ProtocolType::GPIB:
            return gpibAddress >= 0 && gpibAddress <= 30;
        case ProtocolType::TCP_VXI11:
        case ProtocolType::TCP_Socket:
        case ProtocolType::TCP_HiSLIP:
            return !ipAddress.isEmpty();
        case ProtocolType::Serial:
        case ProtocolType::Modbus_RTU:
            return !portName.isEmpty();
        case ProtocolType::Modbus_TCP:
            return !ipAddress.isEmpty();
        case ProtocolType::USB:
            return !vendorId.isEmpty() && !productId.isEmpty();
        default:
            return false;
        }
    }

    // 從資源字串解析（向後相容）
    static CommunicationConfig fromResourceString(const QString& resource) {
        CommunicationConfig config;

        if (resource.isEmpty()) {
            return config;
        }

        // 純數字 -> GPIB
        bool isNumber = false;
        int addr = resource.toInt(&isNumber);
        if (isNumber && addr >= 0 && addr <= 30) {
            config.protocol = ProtocolType::GPIB;
            config.gpibAddress = addr;
            return config;
        }

        // GPIB 格式
        if (resource.startsWith("GPIB", Qt::CaseInsensitive)) {
            config.protocol = ProtocolType::GPIB;
            QRegularExpression rx("GPIB(\\d*)::([\\d]+)(?:::([\\d]+))?::INSTR",
                                   QRegularExpression::CaseInsensitiveOption);
            auto match = rx.match(resource);
            if (match.hasMatch()) {
                config.gpibBoard = match.captured(1).isEmpty() ? 0 : match.captured(1).toInt();
                config.gpibAddress = match.captured(2).toInt();
                if (!match.captured(3).isEmpty())
                    config.gpibSecondary = match.captured(3).toInt();
            }
            return config;
        }

        // TCPIP 格式
        if (resource.startsWith("TCPIP", Qt::CaseInsensitive)) {
            if (resource.contains("SOCKET", Qt::CaseInsensitive)) {
                config.protocol = ProtocolType::TCP_Socket;
            } else if (resource.contains("hislip", Qt::CaseInsensitive)) {
                config.protocol = ProtocolType::TCP_HiSLIP;
            } else {
                config.protocol = ProtocolType::TCP_VXI11;
            }

            QRegularExpression rx("TCPIP\\d*::([\\d\\.]+)(?:::(\\d+))?",
                                   QRegularExpression::CaseInsensitiveOption);
            auto match = rx.match(resource);
            if (match.hasMatch()) {
                config.ipAddress = match.captured(1);
                if (!match.captured(2).isEmpty())
                    config.port = match.captured(2).toInt();
            }
            return config;
        }

        // Modbus 格式
        if (resource.startsWith("MODBUS", Qt::CaseInsensitive)) {
            QRegularExpression rxSlave("SLAVE:(\\d+)", QRegularExpression::CaseInsensitiveOption);
            auto slaveMatch = rxSlave.match(resource);
            if (slaveMatch.hasMatch()) {
                config.slaveId = slaveMatch.captured(1).toInt();
            }

            if (resource.contains("COM", Qt::CaseInsensitive) ||
                resource.contains("/dev/tty", Qt::CaseInsensitive)) {
                config.protocol = ProtocolType::Modbus_RTU;
                // 解析串口參數...
            } else {
                config.protocol = ProtocolType::Modbus_TCP;
                // 解析 TCP 參數...
            }
            return config;
        }

        // Serial 格式 (COM 開頭)
        if (resource.startsWith("COM", Qt::CaseInsensitive) ||
            resource.startsWith("/dev/tty", Qt::CaseInsensitive)) {
            config.protocol = ProtocolType::Serial;
            QStringList parts = resource.split(':');
            if (parts.size() >= 1) config.portName = parts[0];
            if (parts.size() >= 2) config.baudRate = parts[1].toInt();
            if (parts.size() >= 3) config.dataBits = parts[2].toInt();
            if (parts.size() >= 4) config.parity = parts[3];
            if (parts.size() >= 5) config.stopBits = parts[4];
            if (parts.size() >= 6) config.flowControl = parts[5];
            return config;
        }

        // USB 格式
        if (resource.startsWith("USB", Qt::CaseInsensitive)) {
            config.protocol = ProtocolType::USB;
            QRegularExpression rx("USB\\d*::0x([\\dA-Fa-f]+)::0x([\\dA-Fa-f]+)::([^:]+)::INSTR",
                                   QRegularExpression::CaseInsensitiveOption);
            auto match = rx.match(resource);
            if (match.hasMatch()) {
                config.vendorId = match.captured(1);
                config.productId = match.captured(2);
                config.serialNumber = match.captured(3);
            }
            return config;
        }

        return config;
    }

    // XML 序列化
    void writeXml(QXmlStreamWriter& writer) const {
        writer.writeStartElement("CommunicationConfig");
        writer.writeAttribute("protocol", protocolTypeToString(protocol));
        writer.writeAttribute("timeout", QString::number(timeout));

        switch (protocol) {
        case ProtocolType::GPIB:
            writer.writeTextElement("GpibBoard", QString::number(gpibBoard));
            writer.writeTextElement("GpibAddress", QString::number(gpibAddress));
            writer.writeTextElement("GpibSecondary", QString::number(gpibSecondary));
            break;

        case ProtocolType::TCP_VXI11:
        case ProtocolType::TCP_Socket:
        case ProtocolType::TCP_HiSLIP:
            writer.writeTextElement("IpAddress", ipAddress);
            writer.writeTextElement("Port", QString::number(port));
            break;

        case ProtocolType::Serial:
            writer.writeTextElement("PortName", portName);
            writer.writeTextElement("BaudRate", QString::number(baudRate));
            writer.writeTextElement("DataBits", QString::number(dataBits));
            writer.writeTextElement("Parity", parity);
            writer.writeTextElement("StopBits", stopBits);
            writer.writeTextElement("FlowControl", flowControl);
            break;

        case ProtocolType::Modbus_RTU:
            writer.writeTextElement("PortName", portName);
            writer.writeTextElement("BaudRate", QString::number(baudRate));
            writer.writeTextElement("DataBits", QString::number(dataBits));
            writer.writeTextElement("Parity", parity);
            writer.writeTextElement("StopBits", stopBits);
            writer.writeTextElement("SlaveId", QString::number(slaveId));
            break;

        case ProtocolType::Modbus_TCP:
            writer.writeTextElement("IpAddress", ipAddress);
            writer.writeTextElement("Port", QString::number(port));
            writer.writeTextElement("SlaveId", QString::number(slaveId));
            break;

        case ProtocolType::USB:
            writer.writeTextElement("VendorId", vendorId);
            writer.writeTextElement("ProductId", productId);
            writer.writeTextElement("SerialNumber", serialNumber);
            break;

        default:
            break;
        }

        writer.writeEndElement(); // CommunicationConfig
    }

    // XML 反序列化
    void readXml(QXmlStreamReader& reader) {
        if (reader.attributes().hasAttribute("protocol")) {
            protocol = stringToProtocolType(reader.attributes().value("protocol").toString());
        }
        if (reader.attributes().hasAttribute("timeout")) {
            timeout = reader.attributes().value("timeout").toInt();
        }

        while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == QString("CommunicationConfig"))) {
            reader.readNext();
            if (!reader.isStartElement()) continue;

            QString name = reader.name().toString();
            QString text = reader.readElementText();

            if (name == "GpibBoard") gpibBoard = text.toInt();
            else if (name == "GpibAddress") gpibAddress = text.toInt();
            else if (name == "GpibSecondary") gpibSecondary = text.toInt();
            else if (name == "IpAddress") ipAddress = text;
            else if (name == "Port") port = text.toInt();
            else if (name == "PortName") portName = text;
            else if (name == "BaudRate") baudRate = text.toInt();
            else if (name == "DataBits") dataBits = text.toInt();
            else if (name == "Parity") parity = text;
            else if (name == "StopBits") stopBits = text;
            else if (name == "FlowControl") flowControl = text;
            else if (name == "SlaveId") slaveId = text.toInt();
            else if (name == "VendorId") vendorId = text;
            else if (name == "ProductId") productId = text;
            else if (name == "SerialNumber") serialNumber = text;
        }
    }
};
