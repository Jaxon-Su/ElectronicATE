#include "ziparchive.h"

namespace {
// ZIP uses little-endian fields and CRC-32 for each uncompressed entry.
void appendLittleEndian(QByteArray& data, quint32 value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        data.append(char(value >> (8 * i)));
}
quint32 crc32(const QByteArray& data)
{
    quint32 crc = 0xffffffff;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
} // namespace

QByteArray ZipArchive::store(const QList<QPair<QByteArray, QByteArray>>& parts)
{
    QByteArray out, central;
    for (const auto& part : parts) {
        const auto& name = part.first;
        const auto& data = part.second;
        const quint32 offset = out.size(), crc = crc32(data), size = data.size();
        appendLittleEndian(out, 0x04034b50, 4); // Local file header.
        appendLittleEndian(out, 20, 2);
        appendLittleEndian(out, 0, 2);
        appendLittleEndian(out, 0, 2);
        appendLittleEndian(out, 0, 2);
        appendLittleEndian(out, 33, 2);
        appendLittleEndian(out, crc, 4);
        appendLittleEndian(out, size, 4);
        appendLittleEndian(out, size, 4);
        appendLittleEndian(out, name.size(), 2);
        appendLittleEndian(out, 0, 2);
        out += name;
        out += data;
        appendLittleEndian(central, 0x02014b50, 4); // Central directory entry.
        appendLittleEndian(central, 20, 2);
        appendLittleEndian(central, 20, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 33, 2);
        appendLittleEndian(central, crc, 4);
        appendLittleEndian(central, size, 4);
        appendLittleEndian(central, size, 4);
        appendLittleEndian(central, name.size(), 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 2);
        appendLittleEndian(central, 0, 4);
        appendLittleEndian(central, offset, 4);
        central += name;
    }
    const quint32 offset = out.size();
    out += central;
    appendLittleEndian(out, 0x06054b50, 4); // End of central directory.
    appendLittleEndian(out, 0, 2);
    appendLittleEndian(out, 0, 2);
    appendLittleEndian(out, parts.size(), 2);
    appendLittleEndian(out, parts.size(), 2);
    appendLittleEndian(out, central.size(), 4);
    appendLittleEndian(out, offset, 4);
    appendLittleEndian(out, 0, 2);
    return out;
}

// Accept only our generated package layout; refuse edited/corrupt archives rather than overwrite them.
bool ZipArchive::readStored(const QByteArray& data, QList<QPair<QByteArray, QByteArray>>& parts)
{
    parts.clear();
    qsizetype pos = 0;
    auto number = [&](qsizetype offset, int count) {
        quint32 value = 0;
        for (int i = 0; i < count; ++i)
            value |= quint32(quint8(data[offset + i])) << (8 * i);
        return value;
    };
    while (pos + 30 <= data.size() && number(pos, 4) == 0x04034b50) {
        if (number(pos + 6, 2) || number(pos + 8, 2) || number(pos + 28, 2)) return false;
        const auto size = number(pos + 18, 4);
        const auto length = number(pos + 26, 2);
        if (size != number(pos + 22, 4) || quint64(pos) + 30 + length + size > quint64(data.size()))
            return false;
        parts.append({data.mid(pos + 30, length), data.mid(pos + 30 + length, size)});
        pos += 30 + length + size;
        if (parts.size() > 65535) return false;
    }
    return !parts.isEmpty() && store(parts) == data;
}
