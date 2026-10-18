#include "testsequencemodel.h"
#include "testsequencesettingsvalidation.h"
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
bool validateAttributes(QXmlStreamReader &reader, const QStringList &allowed = {})
{
    for (const auto &attribute : reader.attributes()) {
        if (!allowed.contains(attribute.qualifiedName().toString())) {
            reader.raiseError("Unexpected TestSequence attribute: " + attribute.qualifiedName().toString());
            return false;
        }
    }
    return true;
}
}

TestSequenceModel::TestSequenceModel(QObject *parent) : QObject(parent) {}

//  writeXml
//  只儲存 TestSequence 自有的 DUT Test 表格
//  Input / Load / DyLoad / Relay 資料由 TestConditions 負責儲存，不重複寫入
void TestSequenceModel::writeXml(QXmlStreamWriter &writer) const
{
    for (const auto &row : m_tasks) {
        QString error;
        if (!TestSequenceSettingsValidation::retryCount(row.retry)) {
            writer.raiseError("TestSequence Retry must be an integer in 0..100");
            return;
        }
        if (!TestSequenceSettingsValidation::settings(row.item, QJsonObject::fromVariantMap(row.settings), error)) {
            writer.raiseError(error);
            return;
        }
    }
    writer.writeStartElement("TestSequence");

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
        // Settings is required even for an editable draft (for example, a new
        // Write Oscilloscope task). Preserve an empty object instead of omitting it.
        const QByteArray json =
            QJsonDocument(QJsonObject::fromVariantMap(row.settings)).toJson(QJsonDocument::Compact);
        writer.writeTextElement("Settings", QString::fromUtf8(json));
        writer.writeEndElement(); // Row
    }
    writer.writeEndElement(); // DutTable

    writer.writeEndElement(); // TestSequence
}

// Parse into a candidate so direct callers also retain their configuration on failure.
void TestSequenceModel::loadXml(QXmlStreamReader &reader)
{
    QVector<DutRowData> candidate;
    if (!reader.isStartElement() || reader.name() != QLatin1String("TestSequence")) {
        reader.raiseError("Expected TestSequence element");
        return;
    }
    if (!validateAttributes(reader))
        return;
    bool seenTable = false, seenReport = false;
    QString candidateName, candidateDirectory;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("ReportFile") && !seenReport) {
            if (!validateAttributes(reader, {"Name", "Directory"}))
                return;
            seenReport = true;
            const auto attributes = reader.attributes();
            if (!attributes.hasAttribute("Name") || !attributes.hasAttribute("Directory")) {
                reader.raiseError("ReportFile requires Name and Directory");
                return;
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
            reader.raiseError("Unexpected or duplicate TestSequence table");
            return;
        }
        seenTable = true;
        readDutTable(reader, candidate);
        if (reader.hasError())
            return;
    }
    if (reader.hasError())
        return;
    if (!seenTable || !seenReport) {
        reader.raiseError("Missing TestSequence DutTable/ReportFile");
        return;
    }
    for (auto &row : candidate)
        row.uid = m_nextUid++;
    m_tasks = std::move(candidate);
    reportName = candidateName;
    reportDirectory = candidateDirectory;
    emit configLoaded();
}

void TestSequenceModel::readDutTable(QXmlStreamReader &reader, QVector<DutRowData> &rows)
{
    if (!validateAttributes(reader))
        return;
    while (reader.readNextStartElement()) {
        if (reader.name() != QLatin1String("Row")) {
            reader.raiseError("Expected TestSequence Row");
            return;
        }
        DutRowData row;
        if (!validateAttributes(reader, {"Active", "Report"}))
            return;
        const auto active = reader.attributes().value("Active");
        const auto report = reader.attributes().value("Report");
        if ((active != "0" && active != "1") || (report != "0" && report != "1")) {
            reader.raiseError("TestSequence Active/Report must be 0 or 1");
            return;
        }
        row.active = active == "1";
        row.report = report == "1";
        QSet<QString> fields;
        while (reader.readNextStartElement()) {
            if (!validateAttributes(reader))
                return;
            const QString field = reader.name().toString();
            if (fields.contains(field)) {
                reader.raiseError("Duplicate TestSequence field: " + field);
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
                    reader.raiseError("TestSequence Settings must be a valid JSON object: " + error.errorString());
                    return;
                }
                if (!TestSequenceSettingsValidation::uniqueKeys(text)) {
                    reader.raiseError("Duplicate TestSequence JSON key");
                    return;
                }
                const auto settings = document.object();
                row.settings = settings.toVariantMap();
            } else {
                reader.raiseError("Unknown TestSequence field: " + field);
                return;
            }
        }
        if (reader.hasError())
            return;
        if (fields != QSet<QString>{"Item", "Ext", "Retry", "Settings"}) {
            reader.raiseError("Missing TestSequence Item/Ext/Retry/Settings");
            return;
        }
        if (!TestSequenceSettingsValidation::retryCount(row.retry)) {
            reader.raiseError("TestSequence Retry must be an integer in 0..100");
            return;
        }
        QString validationError;
        if (!TestSequenceSettingsValidation::settings(row.item, QJsonObject::fromVariantMap(row.settings),
                                               validationError)) {
            reader.raiseError(validationError);
            return;
        }
        rows.append(row);
    }
}

int TestSequenceModel::appendTask(DutRowData row)
{
    row.uid = m_nextUid++;
    m_tasks.append(std::move(row));
    return m_tasks.last().uid;
}
QVariantMap TestSequenceModel::taskSettings(int uid) const
{
    for (const auto &row : m_tasks) {
        if (row.uid == uid)
            return row.settings;
    }
    return {};
}
void TestSequenceModel::setTaskConfig(int uid, const QString &group, const QVariantMap &config)
{
    for (auto &row : m_tasks) {
        if (row.uid == uid) {
            row.settings[group] = config;
            return;
        }
    }
}
void TestSequenceModel::removeTask(int uid)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].uid == uid) {
            m_tasks.removeAt(i);
            return;
        }
    }
}

void TestSequenceModel::replaceTasks(QVector<DutRowData> rows)
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
