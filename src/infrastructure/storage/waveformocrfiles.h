#pragma once
#include "../../data/waveformocrresult.h"
#include <QStringList>
#include <atomic>
#include <memory>

namespace WaveformOcr
{
using Cancellation = std::shared_ptr<std::atomic_bool>;
struct ScanResult
{
    QStringList paths;
    QString error;
};
ScanResult scanImages(const QString &root, const Cancellation &cancel);
ExportResult exportExcel(const QVector<Row> &rows, const QString &desiredPath, const Cancellation &cancel);
} // namespace WaveformOcr
