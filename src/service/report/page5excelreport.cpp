#include "ziparchive.h"
#include "page5taskkind.h"
#include "page5excelreport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QRegularExpression>
#include <QXmlStreamWriter>
#include <cmath>

namespace {
QString baseName(QString name)
{
    name = name.trimmed();
    if (name.endsWith(".xlsx", Qt::CaseInsensitive))
        name.chop(5);
    return name;
}
} // namespace

bool Page5ExcelReport::validate(const QString& directory, const QString& name, QString& error)
{
    const QString base = baseName(name);
    static const QRegularExpression invalid(R"([<>:"/\\|?*\x00-\x1f])");
    static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))",
                                             QRegularExpression::CaseInsensitiveOption);
    if (base.isEmpty() || base.size() > 120 || base.endsWith('.') || base.endsWith(' ') ||
        invalid.match(base).hasMatch() || reserved.match(base).hasMatch()) {
        error = "Enter a valid Excel file name (without a folder path).";
        return false;
    }
    const QFileInfo folder(directory);
    if (!QDir::isAbsolutePath(directory) || !folder.isDir() || !folder.isWritable()) {
        error = "Select an existing writable report folder.";
        return false;
    }
    error.clear();
    return true;
}

namespace {
QByteArray buildWorksheet(const QVector<Page5ResultRecord>& records, QString& error)
{
    QByteArray sheet;
    QXmlStreamWriter xml(&sheet);
    xml.writeStartDocument();
    xml.writeStartElement("worksheet");
    xml.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    xml.writeStartElement("sheetViews");
    xml.writeStartElement("sheetView");
    xml.writeAttribute("workbookViewId", "0");
    xml.writeStartElement("pane");
    xml.writeAttribute("ySplit", "1");
    xml.writeAttribute("topLeftCell", "A2");
    xml.writeAttribute("state", "frozen");
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeStartElement("cols");
    const QList<int> widths{7, 25, 22, 10, 16, 16, 16, 16, 14, 70};
    for (int i = 0; i < widths.size(); ++i) {
        xml.writeStartElement("col");
        xml.writeAttribute("min", QString::number(i + 1));
        xml.writeAttribute("max", QString::number(i + 1));
        xml.writeAttribute("width", QString::number(widths[i]));
        xml.writeAttribute("customWidth", "1");
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeStartElement("sheetData");
    int row = 0;
    auto writeRow = [&](const QVariantList& cells) {
        xml.writeStartElement("row");
        xml.writeAttribute("r", QString::number(++row));
        for (int col = 0; col < cells.size(); ++col) {
            const auto& cell = cells[col];
            xml.writeStartElement("c");
            xml.writeAttribute("r", QString(QChar('A' + col)) + QString::number(row));
            if (cell.metaType().id() == QMetaType::Double || cell.metaType().id() == QMetaType::Int) {
                xml.writeTextElement("v", QString::number(cell.toDouble(), 'g', 17));
            } else {
                // Inline strings keep user text literal, including leading '=' characters.
                xml.writeAttribute("t", "inlineStr");
                xml.writeStartElement("is");
                xml.writeTextElement("t", cell.toString().left(32767));
                xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        xml.writeEndElement();
    };
    writeRow({"Seq", "Test Item", "Ext. Name", "CH", "Max", "Min", "RMS", "Mean", "Status", "Note"});
    auto value = [](double number) -> QVariant {
        return std::isfinite(number) ? QVariant(number) : QVariant("NA");
    };
    for (const auto& record : records) {
        if (!record.report)
            continue;
        QStringList notes;
        if (record.attempt > 1)
            notes << QString("Retry %1").arg(record.attempt - 1);
        if (!record.summary.isEmpty())
            notes << record.summary;
        if (!record.error.isEmpty())
            notes << record.error;
        notes += record.files;
        const QString note = notes.join("; ");
        qsizetype requiredRows = qMax(qsizetype(1), record.channels.size());
        for (const auto& condition : record.conditions)
            requiredRows += qMax(qsizetype(1), condition.channels.size());
        if (row + requiredRows > 1048576) {
            error = "Excel row limit exceeded.";
            return {};
        }
        if (record.channels.isEmpty()) {
            const bool measurement = Page5TaskKind::isMeasurement(record.taskName);
            const QString missing = measurement ? "NA" : "";
            writeRow({record.taskIndex + 1, record.taskName, record.externalName, "", missing, missing,
                      missing, missing, record.status, note});
        }
        for (const auto& ch : record.channels) {
            QStringList origins;
            auto origin = [&](const QString& metric, const Page5ConditionSource& source) {
                if (source.index >= 0)
                    origins << QString("%1: %2 row %3 (%4)").arg(metric, source.typeName()).arg(source.index + 1).arg(source.label);
            };
            origin("Max", ch.maximumSource);
            origin("Min", ch.minimumSource);
            origin("RMS/Mean last record", ch.latestSource);
            const QString groupNote = record.conditions.isEmpty() ? note
                : "Group summary; " + origins.join("; ") + "; " + note;
            writeRow({record.taskIndex + 1, record.taskName, record.externalName,
                      QString("CH%1%2").arg(ch.channel).arg(ch.unit.isEmpty() ? "" : " (" + ch.unit + ")"),
                      value(ch.maximum), value(ch.minimum), value(ch.rms), value(ch.mean), record.status,
                      groupNote});
        }
        for (const auto& condition : record.conditions) {
            const QString conditionNote = QString("%1 row %2 (%3); %4").arg(condition.condition.typeName())
                                               .arg(condition.condition.index + 1)
                                               .arg(condition.condition.label, condition.error);
            if (condition.channels.isEmpty())
                writeRow({record.taskIndex + 1, record.taskName, record.externalName, "", "NA", "NA",
                          "NA", "NA", condition.status, conditionNote});
            for (const auto& ch : condition.channels)
                writeRow({record.taskIndex + 1, record.taskName, record.externalName,
                          QString("CH%1").arg(ch.channel), value(ch.maximum), value(ch.minimum),
                          value(ch.rms), value(ch.mean), condition.status, conditionNote});
        }
    }
    xml.writeEndElement();
    xml.writeStartElement("autoFilter");
    xml.writeAttribute("ref", "A1:J" + QString::number(row));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) {
        error = "Invalid text in Excel report.";
        return {};
    }
    return sheet;
}
} // namespace

bool Page5ExcelReport::save(const QString& directory, const QString& name,
                            const QVector<Page5ResultRecord>& records, QString& savedPath, QString& error)
{
    savedPath.clear();
    if (!validate(directory, name, error))
        return false;
    const QByteArray sheet = buildWorksheet(records, error);
    if (sheet.isEmpty())
        return false;
    QList<QPair<QByteArray, QByteArray>> parts{
        {"[Content_Types].xml",
          R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)"},
         {"_rels/.rels",
          R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)"},
         {"xl/workbook.xml",
          R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Results" sheetId="1" r:id="rId1"/></sheets></workbook>)"},
         {"xl/_rels/workbook.xml.rels",
          R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>)"},
         {"xl/worksheets/sheet1.xml", sheet}};

    const QString path = QDir(directory).filePath(baseName(name) + ".xlsx");
    QLockFile lock(path + ".lock");
    if (!lock.tryLock()) {
        error = "Report is being written by another process.";
        return false;
    }
    if (QFileInfo::exists(path)) {
        QFile previous(path);
        QList<QPair<QByteArray, QByteArray>> oldParts;
        if (!previous.open(QIODevice::ReadOnly) || previous.size() > 256 * 1024 * 1024 ||
            !ZipArchive::readStored(previous.readAll(), oldParts) || oldParts.size() != parts.size()) {
            error = "Cannot append: existing report is unreadable, edited or unsupported. Choose another file name.";
            return false;
        }
        previous.close();
        for (int i = 0; i < parts.size() - 1; ++i) {
            if (parts[i] != oldParts[i]) {
                error = "Existing file is not an ElectronicATE report. Choose another file name.";
                return false;
            }
        }
        QDomDocument oldSheet, newSheet;
        if (oldParts.last().first != parts.last().first || !oldSheet.setContent(oldParts.last().second) ||
            !newSheet.setContent(sheet)) {
            error = "Existing report worksheet is invalid.";
            return false;
        }
        auto oldData = oldSheet.documentElement().firstChildElement("sheetData");
        const auto newData = newSheet.documentElement().firstChildElement("sheetData");
        if (oldData.isNull() || oldData.firstChildElement("row").isNull()) {
            error = "Existing report has no result header.";
            return false;
        }
        int row = 0;
        for (auto item = oldData.firstChildElement("row"); !item.isNull(); item = item.nextSiblingElement("row")) {
            if (item.attribute("r").toInt() != ++row) {
                error = "Existing report row sequence is invalid.";
                return false;
            }
        }
        for (auto item = newData.firstChildElement("row").nextSiblingElement("row");
             !item.isNull(); item = item.nextSiblingElement("row")) {
            if (++row > 1048576) {
                error = "Excel row limit exceeded.";
                return false;
            }
            auto appended = oldSheet.importNode(item, true).toElement();
            appended.setAttribute("r", row);
            int column = 0;
            for (auto cell = appended.firstChildElement("c"); !cell.isNull(); cell = cell.nextSiblingElement("c"))
                cell.setAttribute("r", QString(QChar('A' + column++)) + QString::number(row));
            oldData.appendChild(appended);
        }
        oldSheet.documentElement().firstChildElement("autoFilter").setAttribute("ref", "A1:J" + QString::number(row));
        parts.last().second = oldSheet.toByteArray(-1);
    }
    const QByteArray data = ZipArchive::store(parts);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    savedPath = path;
    return true;
}
