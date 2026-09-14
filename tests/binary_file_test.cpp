#include "binaryfilestore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

QByteArray read(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read failed");
    return file.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        const QString path = dir.filePath(QString::fromUtf8("擷取.bin"));
        const QByteArray bytes("\x00\x01\xff\r\n\x00", 6);
        auto result = BinaryFileStore::save(path, bytes);
        require(result.succeeded() && result.bytesWritten == bytes.size() && read(path) == bytes,
                "binary data changed");
        result = BinaryFileStore::save(path, "x");
        require(result.succeeded() && read(path) == "x", "replacement retained trailing bytes");
        result = BinaryFileStore::save(path, {});
        require(result.succeeded() && result.bytesWritten == 0 && read(path).isEmpty(), "empty payload failed");
#ifdef Q_OS_WIN
        require(BinaryFileStore::save(path, bytes).succeeded(), "protected fixture failed");
        require(QFile::setPermissions(path, QFileDevice::ReadOwner), "cannot protect fixture");
        result = BinaryFileStore::save(path, "replacement");
        const bool restoredPermissions = QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        require(restoredPermissions, "cannot restore fixture permissions");
        require(!result.succeeded() && read(path) == bytes, "failed replacement damaged read-only file");
#endif
        result = BinaryFileStore::save(dir.filePath("missing/output.bin"), bytes);
        require(result.error == BinaryFileResult::Error::Open && !result.detail.isEmpty(), "missing directory accepted");
        require(QDir(dir.path()).mkdir("protected"), "fixture directory failed");
        const QString sentinel = dir.filePath("protected/keep.bin");
        require(BinaryFileStore::save(sentinel, bytes).succeeded(), "sentinel write failed");
        result = BinaryFileStore::save(dir.filePath("protected"), "replacement");
        require(!result.succeeded() && read(sentinel) == bytes, "failed write damaged destination");
        require(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size() == 1,
                "temporary file leaked");
        std::cout << "PASS: binary bytes, replacement, empty payload and failed destinations\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
