#include "binaryfilestore.h"
#include <QSaveFile>

BinaryFileResult BinaryFileStore::save(const QString& path, const QByteArray& data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {BinaryFileResult::Error::Open, file.errorString()};
    const qint64 written = file.write(data);
    if (written != data.size()) {
        const QString detail = file.errorString();
        file.cancelWriting();
        return {BinaryFileResult::Error::Write, detail};
    }
    if (!file.commit())
        return {BinaryFileResult::Error::Commit, file.errorString()};
    return {BinaryFileResult::Error::None, {}, written};
}
