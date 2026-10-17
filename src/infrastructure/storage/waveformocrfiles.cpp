#include "waveformocrfiles.h"
#include "../../service/waveformocr/waveformocrparser.h"
#include "ziparchive.h"
#include <QCollator>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamWriter>
#include <algorithm>

namespace WaveformOcr
{
ScanResult scanImages(const QString &root, const Cancellation &cancel)
{
    ScanResult result;
    if (!QFileInfo(root).isDir())
    {
        result.error = "圖片資料夾不存在";
        return result;
    }
    QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    const QStringList extensions{"png", "jpg", "jpeg", "bmp", "webp"};
    while (it.hasNext())
    {
        if (cancel->load())
        {
            result.error = "已停止掃描";
            return result;
        }
        it.next();
        if (!extensions.contains(it.fileInfo().suffix().toLower()))
            continue;
        result.paths << it.fileInfo().absoluteFilePath();
        if (result.paths.size() > 10000)
        {
            result.paths.clear();
            result.error = "超過 10,000 張圖片，請縮小資料夾範圍";
            return result;
        }
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(result.paths.begin(), result.paths.end(),
              [&collator](const auto &a, const auto &b) { return collator.compare(a, b) < 0; });
    if (result.paths.isEmpty())
        result.error = "資料夾內沒有支援的圖片";
    return result;
}

namespace
{
const QString ns = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
const QString relNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
QString excelText(QString text)
{
    // XML 1.0 and Excel cell limits. Keep text literal, including '=' and '_xHHHH_' sequences.
    text.replace(QRegularExpression("_(x[0-9A-Fa-f]{4}_)"), "_x005F_\\1");
    text.remove(
        QRegularExpression("[\\x{0000}-\\x{0008}\\x{000B}\\x{000C}\\x{000E}-\\x{001F}\\x{FFFE}\\x{FFFF}]"));
    if (text.size() > 32760)
        text = text.left(32740) + " [truncated]";
    if (!text.isEmpty() && text.back().isHighSurrogate())
        text.chop(1);
    return text;
}
QString uniqueSheet(QString folder, QSet<QString> &used)
{
    if (folder == "." || folder.isEmpty())
        folder = "Root";
    folder.replace(QRegularExpression("[\\\\/?:*\\[\\]\\x{0000}-\\x{001F}]"), "_");
    folder = folder.trimmed();
    while (folder.startsWith('\''))
        folder.remove(0, 1);
    while (folder.endsWith('\''))
        folder.chop(1);
    if (folder.isEmpty())
        folder = "Sheet";
    for (int n = 1;; ++n)
    {
        const QString suffix = n == 1 ? QString() : QString(" (%1)").arg(n);
        QString name = folder.left(31 - suffix.size());
        if (!name.isEmpty() && name.back().isHighSurrogate())
            name.chop(1);
        name += suffix;
        if (name.endsWith('\''))
            name[name.size() - 1] = '_';
        if (name.compare("History", Qt::CaseInsensitive) == 0)
            name = "History_";
        if (!used.contains(name.toCaseFolded()))
        {
            used.insert(name.toCaseFolded());
            return name;
        }
    }
}
QString column(int index)
{
    QString result;
    do
    {
        result.prepend(QChar('A' + index % 26));
        index = index / 26 - 1;
    } while (index >= 0);
    return result;
}
void cell(QXmlStreamWriter &xml, int row, int col, const QString &text,
          const std::optional<double> &number = {})
{
    xml.writeStartElement("c");
    xml.writeAttribute("r", column(col) + QString::number(row));
    if (number)
        xml.writeTextElement("v", QString::number(*number, 'g', 17));
    else
    {
        xml.writeAttribute("t", "inlineStr");
        xml.writeStartElement("is");
        xml.writeStartElement("t");
        xml.writeAttribute("xml:space", "preserve");
        xml.writeCharacters(excelText(text));
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
}
QByteArray sheet(const QVector<Row> &rows, const QList<int> &indices, const Cancellation &cancel)
{
    QByteArray bytes;
    QXmlStreamWriter xml(&bytes);
    xml.writeStartDocument();
    xml.writeStartElement("worksheet");
    xml.writeDefaultNamespace(ns);
    xml.writeStartElement("sheetViews");
    xml.writeStartElement("sheetView");
    xml.writeAttribute("workbookViewId", "0");
    xml.writeEmptyElement("pane");
    xml.writeAttribute("ySplit", "1");
    xml.writeAttribute("topLeftCell", "A2");
    xml.writeAttribute("state", "frozen");
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeStartElement("cols");
    xml.writeEmptyElement("col");
    xml.writeAttribute("min", "1");
    xml.writeAttribute("max", "21");
    xml.writeAttribute("width", "20");
    xml.writeAttribute("customWidth", "1");
    xml.writeEndElement();
    xml.writeStartElement("sheetData");
    QStringList headings{"Folder", "Image",    "Channel",  "Scope",           "Max",          "Max unit",
                         "Min",    "Min unit", "RMS",      "RMS unit",        "Mean",         "Mean unit",
                         "Status", "Issues",   "Reviewed", "Original values", "Raw response", "Source path",
                         "Note", "Manually edited", "Original OCR error"};
    xml.writeStartElement("row");
    xml.writeAttribute("r", "1");
    for (int i = 0; i < headings.size(); ++i)
        cell(xml, 1, i, headings[i]);
    xml.writeEndElement();
    int r = 1;
    for (int index : indices)
    {
        if (cancel->load())
            return {};
        const auto &row = rows[index];
        ++r;
        xml.writeStartElement("row");
        xml.writeAttribute("r", QString::number(r));
        cell(xml, r, 0, row.folder);
        cell(xml, r, 1, QFileInfo(row.imagePath).fileName());
        cell(xml, r, 2, row.channel);
        cell(xml, r, 3, row.scope);
        for (int i = 0; i < 4; ++i)
        {
            cell(xml, r, 4 + i * 2, {}, row.values[i].number);
            cell(xml, r, 5 + i * 2, row.values[i].number ? row.values[i].unit : QString());
        }
        cell(xml, r, 12, status(row));
        cell(xml, r, 13, issues(row).join("; "));
        cell(xml, r, 14, row.reviewed ? "Yes" : "No");
        cell(xml, r, 15, row.originalValues);
        cell(xml, r, 16, row.rawResponse);
        cell(xml, r, 17, row.imagePath);
        cell(xml, r, 18, row.note);
        cell(xml, r, 19, row.modified ? "Yes" : "No");
        cell(xml, r, 20, row.error);
        xml.writeEndElement();
        if (bytes.size() > 128 * 1024 * 1024)
            return {};
    }
    xml.writeEndElement();
    xml.writeEmptyElement("autoFilter");
    xml.writeAttribute("ref", QString("A1:U%1").arg(r));
    xml.writeEndElement();
    xml.writeEndDocument();
    return bytes;
}
} // namespace

ExportResult exportExcel(const QVector<Row> &rows, const QString &desiredPath, const Cancellation &cancel)
{
    if (rows.isEmpty())
        return {{}, "沒有可匯出的結果"};
    QMap<QString, QList<int>> folders;
    for (int i = 0; i < rows.size(); ++i)
        folders[rows[i].folder] << i;
    QList<QPair<QByteArray, QByteArray>> parts;
    QByteArray workbook, relationships, contentTypes;
    QXmlStreamWriter wb(&workbook), rels(&relationships), types(&contentTypes);
    wb.writeStartDocument();
    wb.writeStartElement("workbook");
    wb.writeDefaultNamespace(ns);
    wb.writeNamespace(relNs, "r");
    wb.writeStartElement("sheets");
    rels.writeStartDocument();
    rels.writeStartElement("Relationships");
    rels.writeDefaultNamespace("http://schemas.openxmlformats.org/package/2006/relationships");
    types.writeStartDocument();
    types.writeStartElement("Types");
    types.writeDefaultNamespace("http://schemas.openxmlformats.org/package/2006/content-types");
    types.writeEmptyElement("Default");
    types.writeAttribute("Extension", "rels");
    types.writeAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml");
    types.writeEmptyElement("Default");
    types.writeAttribute("Extension", "xml");
    types.writeAttribute("ContentType", "application/xml");
    types.writeEmptyElement("Override");
    types.writeAttribute("PartName", "/xl/workbook.xml");
    types.writeAttribute("ContentType",
                         "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
    QSet<QString> used;
    int id = 0;
    qint64 total = 0;
    for (auto it = folders.begin(); it != folders.end(); ++it)
    {
        if (cancel->load())
            return {{}, "已取消匯出"};
        if (it.value().size() > 1048575)
            return {{}, "工作表超過 Excel 列數上限"};
        ++id;
        const QString name = uniqueSheet(it.key(), used), rid = QString("rId%1").arg(id);
        const QString path = QString("worksheets/sheet%1.xml").arg(id);
        wb.writeEmptyElement("sheet");
        wb.writeAttribute("name", name);
        wb.writeAttribute("sheetId", QString::number(id));
        wb.writeAttribute("r:id", rid);
        rels.writeEmptyElement("Relationship");
        rels.writeAttribute("Id", rid);
        rels.writeAttribute("Type", relNs + "/worksheet");
        rels.writeAttribute("Target", path);
        types.writeEmptyElement("Override");
        types.writeAttribute("PartName", "/xl/" + path);
        types.writeAttribute("ContentType",
                             "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
        auto bytes = sheet(rows, it.value(), cancel);
        total += bytes.size();
        if (cancel->load())
            return {{}, "已取消匯出"};
        if (bytes.isEmpty() || total > 256 * 1024 * 1024)
            return {{}, "報表過大，請分批匯出"};
        parts.append({("xl/" + path).toUtf8(), bytes});
    }
    wb.writeEndElement();
    wb.writeEndElement();
    wb.writeEndDocument();
    rels.writeEndElement();
    rels.writeEndDocument();
    types.writeEndElement();
    types.writeEndDocument();
    parts.append({"xl/workbook.xml", workbook});
    parts.append({"xl/_rels/workbook.xml.rels", relationships});
    parts.append({"[Content_Types].xml", contentTypes});
    parts.append({"_rels/.rels",
                  QByteArray("<?xml version=\"1.0\"?><Relationships "
                             "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
                             "relationships\"><Relationship Id=\"rId1\" "
                             "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                             "officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>")});
    const auto archive = ZipArchive::store(parts);
    if (cancel->load())
        return {{}, "已取消匯出"};
    QFileInfo requested(desiredPath.endsWith(".xlsx", Qt::CaseInsensitive) ? desiredPath
                                                                           : desiredPath + ".xlsx");
    if (!requested.dir().exists())
        return {{}, "匯出資料夾不存在"};
    // NewOnly reserves the final name atomically; an existing workbook is never opened for writing.
    for (int n = 1; n <= 10000; ++n)
    {
        const QString suffix = n == 1 ? QString() : QString("_%1").arg(n);
        const QString path = requested.dir().filePath(requested.completeBaseName() + suffix + ".xlsx");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            if (QFileInfo::exists(path))
                continue;
            return {{}, "無法建立 Excel: " + file.errorString()};
        }
        if (cancel->load() || file.write(archive) != archive.size() || !file.flush())
        {
            const QString error = cancel->load() ? "已取消匯出" : "Excel 寫入失敗: " + file.errorString();
            file.close();
            file.remove();
            return {{}, error};
        }
        file.close();
        return {path, {}};
    }
    return {{}, "同名檔案過多，請改用其他名稱"};
}
} // namespace WaveformOcr
