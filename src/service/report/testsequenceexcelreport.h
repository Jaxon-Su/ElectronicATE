#pragma once
#include "testsequenceresultrecord.h"
namespace TestSequenceExcelReport {
bool validate(const QString& directory, const QString& name, QString& error);
bool save(const QString& directory, const QString& name, const QVector<TestSequenceResultRecord>& records,
          QString& savedPath, QString& error);
} // namespace TestSequenceExcelReport
