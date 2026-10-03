#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>
#include <optional>

namespace WaveformOcr
{
inline const std::array<QString, 4> Metrics{"Max", "Min", "RMS", "Mean"};
struct Value
{
    QString raw;
    std::optional<double> number; // Normalized SI value; missing is never zero.
    QString unit;
    QString issue;
};
struct Row
{
    QString imagePath;
    QString folder;
    QString channel;
    QString scope; // current / all acquisitions / unspecified, as visible in the image.
    std::array<Value, 4> values;
    QString originalValues;
    QString rawResponse;
    QString error;
    QString note;
    bool reviewed = false;
};
struct ExportResult
{
    QString path;
    QString error;
};
} // namespace WaveformOcr
