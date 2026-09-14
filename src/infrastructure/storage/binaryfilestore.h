#pragma once
#include <QByteArray>
#include <QString>

struct BinaryFileResult {
    enum class Error { None, Open, Write, Commit };
    Error error = Error::None;
    QString detail;
    qint64 bytesWritten = 0;
    bool succeeded() const { return error == Error::None; }
};

namespace BinaryFileStore {
// Replace only after the complete payload has been written successfully.
BinaryFileResult save(const QString& path, const QByteArray& data);
}
