#include "testsequenceresultrows.h"
#include "ziparchive.h"
#include "testsequencetaskkind.h"
#include "testsequenceexcelreport.h"
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

bool TestSequenceExcelReport::validate(const QString& directory, const QString& name, QString& error)
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
QByteArray buildWorksheet(const QVector<TestSequenceResultRecord> &records, bool summary, QString &error)
{
    QByteArray sheet;
    QXmlStreamWriter xml(&sheet);
    xml.writeStartDocument();
    xml.writeStartElement("worksheet");
    xml.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    xml.writeStartElement("sheetViews"); xml.writeStartElement("sheetView");
    xml.writeAttribute("workbookViewId", "0");
    xml.writeStartElement("pane"); xml.writeAttribute("ySplit", "1");
    xml.writeAttribute("topLeftCell", "A2"); xml.writeAttribute("state", "frozen");
    xml.writeEndElement(); xml.writeEndElement(); xml.writeEndElement();
    xml.writeStartElement("cols");
    const QList<int> widths{7, 25, 22, 10, 16, 16, 16, 16, 14, 60, 38, 10, 30, 12, 10, 10, 10, 18, 10};
    for (int i = 0; i < widths.size(); ++i) {
        xml.writeStartElement("col"); xml.writeAttribute("min", QString::number(i + 1));
        xml.writeAttribute("max", QString::number(i + 1)); xml.writeAttribute("width", QString::number(widths[i]));
        xml.writeAttribute("customWidth", "1"); xml.writeEndElement();
    }
    xml.writeEndElement(); xml.writeStartElement("sheetData");
    int row = 0;
    const auto writeRow = [&](const QVariantList &cells) {
        xml.writeStartElement("row"); xml.writeAttribute("r", QString::number(++row));
        for (int col = 0; col < cells.size(); ++col) {
            const auto &cell = cells[col];
            xml.writeStartElement("c");
            xml.writeAttribute("r", QString(QChar('A' + col)) + QString::number(row));
            if (cell.metaType().id() == QMetaType::Double || cell.metaType().id() == QMetaType::Int)
                xml.writeTextElement("v", QString::number(cell.toDouble(), 'g', 17));
            else {
                xml.writeAttribute("t", "inlineStr"); xml.writeStartElement("is");
                xml.writeTextElement("t", cell.toString().left(32767)); xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        xml.writeEndElement();
    };
    QVariantList header;
    for (const auto &name : TestSequenceResultRows::headers()) header.append(name);
    writeRow(header);
    for (const auto &record : records) {
        if (!record.report) continue;
        const auto rows = TestSequenceResultRows::rows(record, summary);
        if (qint64(row) + rows.size() > 1048576) { error = "Excel row limit exceeded."; return {}; }
        for (const auto &values : rows) writeRow(values);
    }
    xml.writeEndElement(); xml.writeStartElement("autoFilter");
    xml.writeAttribute("ref", "A1:S" + QString::number(row)); xml.writeEndElement();
    xml.writeEndElement(); xml.writeEndDocument();
    if (xml.hasError()) { error = "Invalid text in Excel report."; return {}; }
    return sheet;
}

bool appendWorksheet(const QByteArray &oldBytes, QByteArray &newBytes, QString &error)
{
    QDomDocument oldSheet, newSheet;
    if (!oldSheet.setContent(oldBytes) || !newSheet.setContent(newBytes)) {
        error = "Existing report worksheet is invalid."; return false;
    }
    auto oldData = oldSheet.documentElement().firstChildElement("sheetData");
    const auto newData = newSheet.documentElement().firstChildElement("sheetData");
    const auto header = [](QDomElement data) {
        QStringList cells;
        for (auto c = data.firstChildElement("row").firstChildElement("c"); !c.isNull(); c = c.nextSiblingElement("c"))
            cells.append(c.text());
        return cells;
    };
    if (oldData.isNull() || header(oldData) != TestSequenceResultRows::headers() || header(newData) != header(oldData)) {
        error = "Report columns differ. Choose a new file name; the existing report is unchanged."; return false;
    }
    int row = 0;
    for (auto item = oldData.firstChildElement("row"); !item.isNull(); item = item.nextSiblingElement("row")) {
        if (item.attribute("r").toInt() != ++row) { error = "Existing report row sequence is invalid."; return false; }
    }
    for (auto item = newData.firstChildElement("row").nextSiblingElement("row"); !item.isNull(); item = item.nextSiblingElement("row")) {
        if (++row > 1048576) { error = "Excel row limit exceeded."; return false; }
        auto appended = oldSheet.importNode(item, true).toElement(); appended.setAttribute("r", row);
        int col = 0;
        for (auto c = appended.firstChildElement("c"); !c.isNull(); c = c.nextSiblingElement("c"))
            c.setAttribute("r", QString(QChar('A' + col++)) + QString::number(row));
        oldData.appendChild(appended);
    }
    oldSheet.documentElement().firstChildElement("autoFilter").setAttribute("ref", "A1:S" + QString::number(row));
    newBytes = oldSheet.toByteArray(-1);
    return true;
}
} // namespace

bool TestSequenceExcelReport::save(const QString &directory, const QString &name,
                           const QVector<TestSequenceResultRecord> &records, QString &savedPath, QString &error)
{
    savedPath.clear();
    if (!validate(directory, name, error)) return false;
    const auto summary = buildWorksheet(records, true, error);
    const auto trials = buildWorksheet(records, false, error);
    if (summary.isEmpty() || trials.isEmpty()) return false;
    QList<QPair<QByteArray, QByteArray>> parts{
        {"[Content_Types].xml", R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)"},
        {"_rels/.rels", R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)"},
        {"xl/workbook.xml", R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Results" sheetId="1" r:id="rId1"/><sheet name="Trials" sheetId="2" r:id="rId2"/></sheets></workbook>)"},
        {"xl/_rels/workbook.xml.rels", R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/></Relationships>)"},
        {"xl/worksheets/sheet1.xml", summary}, {"xl/worksheets/sheet2.xml", trials}
    };
    const QString path = QDir(directory).filePath(baseName(name) + ".xlsx");
    QLockFile lock(path + ".lock");
    if (!lock.tryLock()) { error = "Report is being written by another process."; return false; }
    if (QFileInfo::exists(path)) {
        QFile previous(path);
        QList<QPair<QByteArray, QByteArray>> oldParts;
        if (!previous.open(QIODevice::ReadOnly) || previous.size() > 256 * 1024 * 1024 ||
            !ZipArchive::readStored(previous.readAll(), oldParts) || oldParts.size() != parts.size()) {
            error = "Cannot append: existing report uses another format or is unreadable. Choose a new file name."; return false;
        }
        previous.close();
        for (int i = 0; i < parts.size(); ++i) {
            if (parts[i].first != oldParts[i].first) { error = "Existing report package is unsupported."; return false; }
            if (parts[i].first.startsWith("xl/worksheets/")) {
                if (!appendWorksheet(oldParts[i].second, parts[i].second, error)) return false;
            } else if (parts[i] != oldParts[i]) {
                error = "Existing file is not this ElectronicATE report format. Choose a new file name."; return false;
            }
        }
    }
    const auto bytes = ZipArchive::store(parts);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = file.errorString(); return false;
    }
    savedPath = path;
    return true;
}
