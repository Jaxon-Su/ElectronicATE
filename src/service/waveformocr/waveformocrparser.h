#pragma once
#include "../../data/waveformocrresult.h"

namespace WaveformOcr
{
Value parseValue(const QString &text);
QVector<Row> parseText(const QString &imagePath, const QString &root, const QString &text);
Row failedRow(const QString &imagePath, const QString &root, const QString &error);
QStringList issues(const Row &row);
QString status(const Row &row);
QString displayValue(const Value &value);
QString recognitionPrompt();
} // namespace WaveformOcr
