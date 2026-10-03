#include "page5model.h"
#include "page5settingsvalidation.h"
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>

Page5Model::Page5Model(QObject *parent) : QObject(parent) {}

//  writeXml
//  只儲存 Page5 自有的 DUT Test 表格
//  Input / Load / DyLoad / Relay 資料由 Page2 負責儲存，不重複寫入
void Page5Model::writeXml(QXmlStreamWriter &writer) const
{
    for (const auto &row : m_tasks) {
        QString error;
        if (!Page5SettingsValidation::retryCount(row.retry)) {
            writer.raiseError("Page5 Retry must be an integer in 0..100");
            return;
        }
        if (!Page5SettingsValidation::settings(row.item, QJsonObject::fromVariantMap(row.settings), error)) {
            writer.raiseError(error);
            return;
        }
    }
    writer.writeStartElement("Page5");

    writer.writeStartElement("ReportFile");
    writer.writeAttribute("Name", reportName);
    writer.writeAttribute("Directory", reportDirectory);
    writer.writeEndElement();
    writer.writeStartElement("DutTable");
    for (const auto &row : m_tasks) {
        writer.writeStartElement("Row");
        writer.writeAttribute("Active", row.active ? "1" : "0");
        writer.writeAttribute("Report", row.report ? "1" : "0");
        writer.writeTextElement("Item", row.item);
        writer.writeTextElement("Ext", row.ext);
        writer.writeTextElement("Retry", row.retry);
        if (!row.settings.isEmpty()) {
            const QByteArray json = QJsonDocument::fromVariant(row.settings).toJson(QJsonDocument::Compact);
            writer.writeTextElement("Settings", QString::fromUtf8(json));
        }
        writer.writeEndElement(); // Row
    }
    writer.writeEndElement(); // DutTable

    writer.writeEndElement(); // Page5
}

// Parse into a candidate so direct callers also retain their configuration on failure.
void Page5Model::loadXml(QXmlStreamReader &reader)
{
    QVector<DutRowData> candidate;
    if (!reader.isStartElement() || reader.name() != QLatin1String("Page5")) {
        reader.raiseError("Expected Page5 element");
        return;
    }
    bool seenTable = false, seenReport = false;
    QString candidateName, candidateDirectory;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("ReportFile") && !seenReport) {
            seenReport = true;
            const auto attributes = reader.attributes();
            for (const auto &attribute : attributes) {
                if (attribute.name() != QLatin1String("Name") &&
                    attribute.name() != QLatin1String("Directory")) {
                    reader.raiseError("Unexpected ReportFile attribute");
                    return;
                }
            }
            candidateName = attributes.value("Name").toString();
            candidateDirectory = attributes.value("Directory").toString();
            if (!reader.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).trimmed().isEmpty())
                reader.raiseError("ReportFile must be empty");
            if (reader.hasError())
                return;
            continue;
        }
        if (reader.name() != QLatin1String("DutTable") || seenTable) {
            reader.raiseError("Unexpected or duplicate Page5 table");
            return;
        }
        seenTable = true;
        readDutTable(reader, candidate);
        if (reader.hasError())
            return;
    }
    if (reader.hasError())
        return;
    if (!seenTable) {
        reader.raiseError("Missing Page5 DutTable");
        return;
    }
    for (auto &row : candidate)
        row.uid = m_nextUid++;
    m_tasks = std::move(candidate);
    reportName = candidateName;
    reportDirectory = candidateDirectory;
    emit configLoaded();
}

void Page5Model::readDutTable(QXmlStreamReader &reader, QVector<DutRowData> &rows)
{
    while (reader.readNextStartElement()) {
        if (reader.name() != QLatin1String("Row")) {
            reader.raiseError("Expected Page5 Row");
            return;
        }
        DutRowData row;
        const auto active = reader.attributes().value("Active");
        const auto report = reader.attributes().value("Report");
        if ((active != "0" && active != "1") || (report != "0" && report != "1")) {
            reader.raiseError("Page5 Active/Report must be 0 or 1");
            return;
        }
        row.active = active == "1";
        row.report = report == "1";
        QSet<QString> fields;
        while (reader.readNextStartElement()) {
            const QString field = reader.name().toString();
            if (fields.contains(field)) {
                reader.raiseError("Duplicate Page5 field: " + field);
                return;
            }
            fields.insert(field);
            const QString text = reader.readElementText();
            if (reader.hasError())
                return;
            if (field == "Item")
                row.item = text;
            else if (field == "Ext")
                row.ext = text;
            else if (field == "Retry")
                row.retry = text;
            else if (field == "Settings") {
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
                if (error.error != QJsonParseError::NoError || !document.isObject()) {
                    reader.raiseError("Page5 Settings must be a valid JSON object: " + error.errorString());
                    return;
                }
                if (!Page5SettingsValidation::uniqueKeys(text)) {
                    reader.raiseError("Duplicate Page5 JSON key");
                    return;
                }
                const auto settings = document.object();
                row.settings = settings.toVariantMap();
            } else {
                reader.raiseError("Unknown Page5 field: " + field);
                return;
            }
        }
        if (reader.hasError())
            return;
        if (!fields.contains("Item") || !fields.contains("Retry")) {
            reader.raiseError("Missing Page5 Item/Retry");
            return;
        }
        if (!Page5SettingsValidation::retryCount(row.retry)) {
            reader.raiseError("Page5 Retry must be an integer in 0..100");
            return;
        }
        QString validationError;
        if (!Page5SettingsValidation::settings(row.item, QJsonObject::fromVariantMap(row.settings),
                                               validationError)) {
            reader.raiseError(validationError);
            return;
        }
        rows.append(row);
    }
}

int Page5Model::appendTask(DutRowData row)
{
    row.uid = m_nextUid++;
    m_tasks.append(std::move(row));
    return m_tasks.last().uid;
}
QVariantMap Page5Model::taskSettings(int uid) const
{
    for (const auto &row : m_tasks) {
        if (row.uid == uid)
            return row.settings;
    }
    return {};
}
void Page5Model::setTaskConfig(int uid, const QString &group, const QVariantMap &config)
{
    for (auto &row : m_tasks) {
        if (row.uid == uid) {
            row.settings[group] = config;
            return;
        }
    }
}
void Page5Model::removeTask(int uid)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].uid == uid) {
            m_tasks.removeAt(i);
            return;
        }
    }
}

void Page5Model::replaceTasks(QVector<DutRowData> rows)
{
    QSet<int> used;
    for (const auto &row : rows)
        m_nextUid = qMax(m_nextUid, row.uid + 1);
    for (auto &row : rows) {
        // Imported drafts and copied rows get a fresh runtime identity.
        if (row.uid < 0 || used.contains(row.uid))
            row.uid = m_nextUid++;
        else
            m_nextUid = qMax(m_nextUid, row.uid + 1);
        used.insert(row.uid);
    }
    m_tasks = std::move(rows);
}
