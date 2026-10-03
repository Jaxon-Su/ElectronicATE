#include "modbusrtucodec.h"
#include "modbusrtuprotocol.h"
#include <QRegularExpression>
#include <QStringList>

namespace {
void appendWord(QByteArray &bytes, int value) {
    bytes.append(char(value >> 8)); bytes.append(char(value));
}
quint16 word(const QByteArray &bytes, int offset) {
    return (quint16(quint8(bytes[offset])) << 8) | quint8(bytes[offset + 1]);
}
QString exceptionName(quint8 code) {
    switch (code) {
    case 1: return "Illegal function";
    case 2: return "Illegal data address";
    case 3: return "Illegal data value";
    case 4: return "Server device failure";
    case 5: return "Acknowledge";
    case 6: return "Server device busy";
    case 8: return "Memory parity error";
    case 10: return "Gateway path unavailable";
    case 11: return "Gateway target failed to respond";
    default: return "Unknown exception";
    }
}
}

bool ModbusRtuCodec::parseValues(const QString &text, QVector<quint16> &values, QString &error)
{
    values.clear(); error.clear();
    const auto tokens = text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (tokens.size() > 1968) { error = "Too many write values"; return false; }
    const QRegularExpression integer("^(?:[0-9]+|0[xX][0-9a-fA-F]+)$");
    for (const auto &token : tokens) {
        bool ok = false;
        const auto value = token.toUInt(&ok, token.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
        if (!integer.match(token).hasMatch() || !ok || value > 65535) {
            error = "Invalid value: " + token + " (use 0..65535 or 0x0000..0xFFFF)";
            values.clear(); return false;
        }
        values.append(quint16(value));
    }
    return true;
}

QByteArray ModbusRtuCodec::encode(const ModbusRequest &r, QString &error)
{
    error.clear();
    auto reject = [&](const QString &message) { error = message; return QByteArray{}; };
    if (r.slave < 1 || r.slave > 247) return reject("Slave ID must be 1..247 (no broadcast)");
    int maximum = 0;
    switch (r.function) {
    case 1: case 2: maximum = 2000; break;
    case 3: case 4: maximum = 125; break;
    case 5: case 6: maximum = 1; break;
    case 15: maximum = 1968; break;
    case 16: maximum = 123; break;
    default: return reject("Unsupported Modbus function");
    }
    if (r.count < 1 || r.count > maximum) return reject(QString("Quantity must be 1..%1").arg(maximum));
    if (r.address < 0 || r.address > 65535 || r.count > 65536 - r.address)
        return reject("Address range must stay within 0x0000..0xFFFF");
    const bool reading = r.function <= 4;
    if ((reading && !r.values.isEmpty()) || (!reading && r.values.size() != r.count))
        return reject("Write value count must match Quantity; read requests must not contain values");
    if (r.function == 5 || r.function == 15)
        for (auto value : r.values) if (value > 1) return reject("Coil values must be 0 (OFF) or 1 (ON)");
    QByteArray frame;
    frame.append(char(r.slave)); frame.append(char(r.function)); appendWord(frame, r.address);
    if (r.function == 5) appendWord(frame, r.values[0] ? 0xFF00 : 0);
    else if (r.function == 6) appendWord(frame, r.values[0]);
    else {
        appendWord(frame, r.count);
        if (r.function == 15) {
            const int bytes = (r.count + 7) / 8;
            frame.append(char(bytes));
            for (int i = 0; i < bytes; ++i) {
                quint8 bits = 0;
                for (int bit = 0; bit < 8 && i * 8 + bit < r.count; ++bit)
                    bits |= quint8(r.values[i * 8 + bit] << bit);
                frame.append(char(bits));
            }
        } else if (r.function == 16) {
            frame.append(char(r.count * 2));
            for (auto value : r.values) appendWord(frame, value);
        }
    }
    const auto crc = Modbus_RTU_Protocol::calcCRC(frame);
    frame.append(char(crc)); frame.append(char(crc >> 8));
    return frame;
}

int ModbusRtuCodec::responseSize(const ModbusRequest &r, const QByteArray &partial)
{
    if (partial.size() >= 2 && (quint8(partial[1]) & 0x80)) return 5;
    if (r.function <= 2) return 5 + (r.count + 7) / 8;
    if (r.function <= 4) return 5 + r.count * 2;
    return 8;
}

bool ModbusRtuCodec::decode(const ModbusRequest &r, const QByteArray &response, QString &summary, QString &error)
{
    summary.clear();
    const auto request = encode(r, error);
    if (request.isEmpty()) return false;
    auto reject = [&](const QString &message) { error = message; return false; };
    if (response.size() != responseSize(r, response)) return reject("Invalid response length");
    const auto crc = Modbus_RTU_Protocol::calcCRC(response.first(response.size() - 2));
    if (quint8(response[response.size() - 2]) != quint8(crc) || quint8(response.back()) != quint8(crc >> 8))
        return reject("CRC mismatch");
    if (quint8(response[0]) != r.slave) return reject("Slave ID mismatch");
    if (quint8(response[1]) == (r.function | 0x80))
        return reject(QString("Modbus exception 0x%1: %2").arg(quint8(response[2]), 2, 16, QChar('0'))
                          .arg(exceptionName(quint8(response[2]))));
    if (quint8(response[1]) != r.function) return reject("Function code mismatch");
    QStringList lines;
    if (r.function <= 4) {
        if (quint8(response[2]) != response.size() - 5) return reject("Byte count mismatch");
        if (r.function <= 2 && r.count % 8 && (quint8(response[response.size() - 3]) >> (r.count % 8)))
            return reject("Non-zero padding bits in coil response");
        for (int i = 0; i < r.count; ++i) {
            const auto address = QString("0x%1").arg(r.address + i, 4, 16, QChar('0'));
            if (r.function <= 2)
                lines << QString("%1 = %2").arg(address).arg((quint8(response[3 + i / 8]) >> (i % 8)) & 1);
            else {
                const auto value = word(response, 3 + i * 2);
                lines << QString("%1 = %2 (0x%3)").arg(address).arg(value).arg(value, 4, 16, QChar('0'));
            }
        }
        summary = lines.join('\n');
    } else {
        // Write responses echo the address and value (05/06) or quantity (0F/10).
        if (response.first(6) != request.first(6)) return reject("Write response does not match address/value/quantity");
        summary = QString("Write acknowledged: %1 value(s), start 0x%2")
                      .arg(r.count).arg(r.address, 4, 16, QChar('0'));
    }
    return true;
}
