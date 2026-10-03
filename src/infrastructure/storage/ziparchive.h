#pragma once
#include <QByteArray>
#include <QList>
#include <QPair>

namespace ZipArchive {
// Uncompressed ZIP entries for small, generated document packages.
QByteArray store(const QList<QPair<QByteArray, QByteArray>>& parts);
bool readStored(const QByteArray& data, QList<QPair<QByteArray, QByteArray>>& parts);
} // namespace ZipArchive
