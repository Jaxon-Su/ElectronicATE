#pragma once
#include <QDir>
#include "captureoptions.h"
#include <QFileInfo>
#include <QSet>
#include <QVariantMap>

inline bool validatePage5Capture(const QVariantMap &cfg, QString &error)
{
    try {
        const CaptureOptions options(cfg);
    } catch (const std::exception &exception) {
        error = QString::fromUtf8(exception.what());
        return false;
    }
    const auto path = cfg.value("directory").toString().trimmed();
    const QFileInfo info(path);
    if (path.isEmpty() || !QDir::isAbsolutePath(path) || !info.isDir() || !info.isWritable()) {
        error = "Select an existing writable absolute capture directory.";
        return false;
    }
    return true;
}
