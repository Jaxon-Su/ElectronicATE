#pragma once
#include <QRegularExpression>
#include <QStringList>
#include <stdexcept>

// Tek 4/5/6 programmer manual: optional response header, quoted entries separated
// by commas; each LDIR entry is name;type;bytes;date;time. Commas in names are data.
namespace MsoFileListing {
inline QStringList quotedEntries(QString text, const QString &header)
{
    text = text.trimmed();
    text.remove(QRegularExpression("^:?FILES(?:YSTEM)?:" + header + "\\s+",
                                   QRegularExpression::CaseInsensitiveOption));
    QStringList entries;
    qsizetype i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i].isSpace()) ++i;
        if (i == text.size() || text[i++] != '"')
            throw std::runtime_error("Invalid quoted filesystem response");
        QString entry;
        bool closed = false;
        while (i < text.size()) {
            const auto c = text[i++];
            if (c != '"') { entry += c; continue; }
            if (i < text.size() && text[i] == '"') { entry += '"'; ++i; continue; }
            closed = true;
            break;
        }
        if (!closed) throw std::runtime_error("Incomplete filesystem response");
        entries.append(entry);
        while (i < text.size() && text[i].isSpace()) ++i;
        if (i == text.size()) break;
        if (text[i++] != ',' || text.mid(i).trimmed().isEmpty())
            throw std::runtime_error("Invalid filesystem entry separator");
    }
    return entries;
}

inline QString currentDirectory(const QByteArray &reply)
{
    const auto entries = quotedEntries(QString::fromUtf8(reply), "CWD");
    if (entries.size() != 1 || entries[0].isEmpty())
        throw std::runtime_error("Invalid FILESystem:CWD response");
    return entries[0];
}

inline qint64 fileSize(const QByteArray &reply, const QString &fileName)
{
    const auto entries = quotedEntries(QString::fromUtf8(reply), "LDIR");
    qint64 size = -1;
    for (const auto &entry : entries) {
        const auto fields = entry.split(';');
        if (fields.size() < 5) continue;
        const int typeIndex = fields.size() - 4;
        if (fields.mid(0, typeIndex).join(';') != fileName) continue;
        bool ok = false;
        const auto bytes = fields[typeIndex + 1].toLongLong(&ok);
        if (size >= 0 || fields[typeIndex].compare("FILE", Qt::CaseInsensitive) != 0 || !ok || bytes <= 0)
            throw std::runtime_error("Invalid or duplicate waveform file entry in LDIR");
        size = bytes;
    }
    if (size < 0) throw std::runtime_error("Waveform file missing from LDIR");
    return size;
}
} // namespace MsoFileListing
