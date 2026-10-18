#include "tekwfmreader.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <new>

namespace Waveform {
namespace {
// Tektronix WFM v3, 077-0220-11; offsets use packed fields, not native C++ structs.
// Checked against the official tm_data_types reader and its analog fixtures.
class Bytes {
public:
    QByteArray data;
    bool little = true;
    void require(quint64 pos, quint64 length) const
    {
        if (pos > quint64(data.size()) || length > quint64(data.size()) - pos)
            throw QStringLiteral("Truncated WFM file or invalid data offset.");
    }
    template<class T> T integer(quint64 pos) const
    {
        require(pos, sizeof(T));
        return little ? qFromLittleEndian<T>(data.constData() + pos) : qFromBigEndian<T>(data.constData() + pos);
    }
    double real(quint64 pos) const
    {
        const auto bits = integer<quint64>(pos);
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    QByteArray raw(quint64 pos, quint64 length) const
    {
        require(pos, length);
        return data.mid(qsizetype(pos), qsizetype(length));
    }
    QString string(quint64 pos, quint64 length) const
    {
        auto bytes = raw(pos, length);
        const auto zero = bytes.indexOf('\0');
        if (zero >= 0) bytes.truncate(zero);
        return QString::fromLatin1(bytes).trimmed();
    }
};

void checkMetadata(const Bytes &b, quint64 pos)
{
    if (pos == quint64(b.data.size())) return;
    if (quint64(b.data.size()) - pos > 2 * 1024 * 1024 || b.raw(pos, 8) != "tekmeta!")
        throw QStringLiteral("Unsupported WFM metadata.");
    pos += 8;
    bool analog = true;
    const auto entries = b.integer<quint32>(pos); pos += 4;
    if (entries > 10000) throw QStringLiteral("Invalid WFM metadata count.");
    for (quint32 i = 0; i < entries; ++i) {
        const auto length = b.integer<quint32>(pos); pos += 4;
        const auto key = b.raw(pos, length); pos += length;
        bool probeNumberValid = false;
        const int probeNumber = key.mid(1).toInt(&probeNumberValid);
        const bool digitalProbe = key.startsWith('d') && probeNumberValid && probeNumber >= 0 && probeNumber <= 15;
        if (key.startsWith("IQ_") || digitalProbe) analog = false;
        const auto type = b.integer<quint8>(pos++);
        quint64 size = 0;
        switch (type) {
        case 1: size = b.integer<quint32>(pos); pos += 4; break;
        case 2: case 4: size = 4; break;
        case 3: size = 8; break;
        default: throw QStringLiteral("Unsupported WFM metadata value type.");
        }
        b.require(pos, size); pos += size;
    }
    // Instrument exports may end with one LF; other trailing bytes remain errors.
    if (pos + 1 == quint64(b.data.size()) && b.raw(pos, 1) == "\n") ++pos;
    if (pos != quint64(b.data.size())) throw QStringLiteral("Unexpected trailing WFM data.");
    if (!analog)
        throw QStringLiteral("IQ and digital WFM files are not supported. Open a single analog YT record.");
}

double sample(const Bytes &b, quint64 pos, quint32 format)
{
    switch (format) {
    case 0: return b.integer<qint16>(pos);
    case 1: return b.integer<qint32>(pos);
    case 2: return b.integer<quint32>(pos);
    case 3: {
        const auto value = b.integer<quint64>(pos);
        if (value > (quint64(1) << 53)) throw QStringLiteral("64-bit sample exceeds exact numeric precision.");
        return double(value);
    }
    case 4: {
        const auto bits = b.integer<quint32>(pos); float value;
        std::memcpy(&value, &bits, sizeof(value)); return value;
    }
    case 5: return b.real(pos);
    case 6: return b.integer<quint8>(pos);
    case 7: return b.integer<qint8>(pos);
    default: throw QStringLiteral("Unsupported WFM sample encoding.");
    }
}
}

Result readTekWfm(const QString &path, const Cancellation &cancel)
{
    if (stopped(cancel)) return {{}, {}, true};
    try {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {{}, file.errorString()};
        constexpr qint64 maxBytes = 512LL * 1024 * 1024;
        if (file.size() < 846 || file.size() > maxBytes)
            return {{}, QStringLiteral("WFM file must be between 846 bytes and 512 MiB.")};
        Bytes b;
        while (!file.atEnd()) {
            if (stopped(cancel)) return {{}, {}, true};
            const auto chunk = file.read(1024 * 1024);
            if (chunk.isEmpty() && file.error() != QFileDevice::NoError) return {{}, file.errorString()};
            if (b.data.size() + chunk.size() > maxBytes) return {{}, QStringLiteral("WFM file exceeds 512 MiB.")};
            b.data.append(chunk);
        }
        const auto magic = b.raw(0, 2);
        if (magic != QByteArray::fromHex("0f0f") && magic != QByteArray::fromHex("f0f0"))
            throw QStringLiteral("Not a Tektronix WFM file.");
        b.little = magic == QByteArray::fromHex("0f0f");
        if (b.raw(2, 8) != ":WFM#003")
            throw QStringLiteral("Only Tektronix WFM version 3 is supported.");
        if (b.integer<quint32>(72) != 0 || b.integer<quint32>(78) != 0 ||
            b.integer<quint32>(82) != 1 || b.integer<quint32>(110) != 1 ||
            b.integer<quint32>(114) != 1 || b.integer<quint32>(118) != 1 ||
            b.integer<quint32>(122) != 2 || b.integer<quint32>(142) != 1 ||
            b.integer<quint32>(146) != 0 || b.integer<quint32>(150) != 0 ||
            b.integer<quint16>(154) != 0 || b.integer<quint32>(244) > 1 ||
            b.integer<quint32>(156) > 1 || b.integer<quint32>(760) != 1 ||
            b.integer<quint32>(764) != 1 || b.integer<quint32>(768) != 0 || b.integer<quint32>(812) != 0)
            throw QStringLiteral("Only a single analog YT Sample or Min/Max record is supported (no FastFrame, summary, histogram, XY or digital).");
        const bool minMaxPairs = b.integer<quint32>(244) == 1;
        const auto format = b.integer<quint32>(240);
        const int sizes[] = {2, 4, 4, 8, 4, 8, 1, 1};
        if (format >= 8 || b.integer<quint8>(15) != sizes[format])
            throw QStringLiteral("Unsupported or inconsistent WFM sample encoding.");
        const quint64 bytesPerPoint = sizes[format];
        const auto buffer = b.integer<qint32>(16);
        const quint64 pre = b.integer<quint32>(818), begin = b.integer<quint32>(822);
        const quint64 end = b.integer<quint32>(826), post = b.integer<quint32>(830);
        const quint64 bufferEnd = b.integer<quint32>(834);
        if (buffer < 838 || pre > begin || begin >= end || end > post || post != bufferEnd ||
            pre % bytesPerPoint || begin % bytesPerPoint || end % bytesPerPoint || post % bytesPerPoint)
            throw QStringLiteral("Invalid WFM curve boundaries.");
        const auto count = (end - begin) / bytesPerPoint;
        if (minMaxPairs && (pre % (2 * bytesPerPoint) || begin % (2 * bytesPerPoint) ||
                            end % (2 * bytesPerPoint) || post % (2 * bytesPerPoint)))
            throw QStringLiteral("Invalid Min/Max pair boundaries.");
        if (count > quint64(MaxSamples) || b.integer<quint32>(504) != count)
            throw QStringLiteral("Invalid WFM point count (maximum 10,000,000).");
        const quint64 checksumOffset = quint64(buffer) + post;
        b.require(checksumOffset, 8);
        quint64 checksum = 0;
        for (quint64 i = 0; i < checksumOffset; ++i) {
            if ((i & 16383) == 0 && stopped(cancel)) return {{}, {}, true};
            checksum += static_cast<unsigned char>(b.data[qsizetype(i)]);
        }
        if (checksum != b.integer<quint64>(checksumOffset))
            throw QStringLiteral("WFM checksum mismatch; the file may be incomplete or damaged.");
        checkMetadata(b, checksumOffset + 8);
        const double scale = b.real(168), offset = b.real(176);
        if (!std::isfinite(scale) || scale == 0 || !std::isfinite(offset) || b.string(508, 20) != "s")
            throw QStringLiteral("Unsupported WFM units or vertical scaling.");
        Trace trace;
        trace.sourcePath = QFileInfo(file).canonicalFilePath();
        trace.label = b.string(40, 32);
        trace.unit = b.string(188, 20);
        trace.start = b.real(496);
        trace.interval = b.real(488);
        trace.minMaxPairs = minMaxPairs;
        trace.samples.resize(qsizetype(count));
        for (quint64 i = 0; i < count; ++i) {
            if ((i & 16383) == 0 && stopped(cancel)) return {{}, {}, true};
            const double raw = sample(b, quint64(buffer) + begin + i * bytesPerPoint, format);
            trace.samples[qsizetype(i)] = raw * scale + offset;
            if (minMaxPairs && (i % 2 == 1)) {
                const double previous = sample(b, quint64(buffer) + begin + (i - 1) * bytesPerPoint, format);
                if (previous > raw) throw QStringLiteral("Invalid Min/Max pair ordering.");
                if (scale < 0) std::swap(trace.samples[qsizetype(i-1)], trace.samples[qsizetype(i)]);
            }
        }
        return summarize(std::move(trace), cancel);
    } catch (const QString &error) {
        return {{}, error};
    } catch (const std::bad_alloc &) {
        return {{}, QStringLiteral("Not enough memory to load this WFM file.")};
    }
}
}
