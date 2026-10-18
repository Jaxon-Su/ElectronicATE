#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <stdexcept>

namespace TestEvidence {
// CTest runs in its build directory. A caller may select an explicit evidence
// directory when collecting a run's artifacts outside that directory.
inline QString path(const QString &name)
{
    QString directory = qEnvironmentVariable("ELECTRONICATE_TEST_EVIDENCE_DIR");
    if (directory.isEmpty())
        directory = QDir::current().filePath("test-evidence");
    const QString result = QDir(directory).filePath(name);
    if (!QDir().mkpath(QFileInfo(result).absolutePath()))
        throw std::runtime_error("Cannot create test evidence directory");
    return result;
}
}
