#pragma once
#include "page5resultrecord.h"
namespace Page5ExcelReport {
bool validate(const QString& directory, const QString& name, QString& error);
bool save(const QString& directory, const QString& name, const QVector<Page5ResultRecord>& records,
          QString& savedPath, QString& error);
} // namespace Page5ExcelReport
