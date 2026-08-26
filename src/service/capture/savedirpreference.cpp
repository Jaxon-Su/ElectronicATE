#include "savedirpreference.h"
#include <QSettings>
#include <QFileInfo>
#include <QStandardPaths>

QString SaveDirPreference::load()
{
    QSettings settings(kOrg, kApp);
    return settings.value(
        kKey,
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
    ).toString();
}

void SaveDirPreference::save(const QString& filePath)
{
    if (filePath.isEmpty()) return;
    QSettings settings(kOrg, kApp);
    settings.setValue(kKey, QFileInfo(filePath).absolutePath());
}
