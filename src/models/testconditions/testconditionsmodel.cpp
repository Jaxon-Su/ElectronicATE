#include "testconditionsmodel.h"
#include <QDebug>
#include <QSet>

TestConditionsModel::TestConditionsModel(QObject* parent) : QObject(parent)
{
}

void TestConditionsModel::resizeLoadOutputs(int count)
{
    if (count <= 0)
        return;
    const auto resize = [count](QVector<QString>& values) { values.resize(count); };
    resize(loadMeta.names);
    resize(loadMeta.vo);
    resize(loadMeta.modes);
    resize(loadMeta.ranges);
    resize(loadMeta.von);
    resize(dynamicMeta.ranges);
    resize(dynamicMeta.vo);
    resize(dynamicMeta.von);
    for (auto &mode : loadMeta.modes) if (mode.isEmpty()) mode = "CC";
    for (auto &range : loadMeta.ranges) if (range.isEmpty()) range = "Auto Range";
    for (auto &range : dynamicMeta.ranges) if (range.isEmpty()) range = "Auto Range";
}

void TestConditionsModel::resizeRelayOutputs(int count)
{
    if (count <= 0)
        return;
    for (auto& row : relayRows) {
        while (row.values.size() < count)
            row.values.append("off");
        row.values.resize(count);
    }
}

// 主要 XML 操作

void TestConditionsModel::writeXml(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("TestConditions");
    // Equal row indices across the three source tables form one DC condition.
    writer.writeAttribute("schemaVersion", "2");

    XmlWriter::writeInputTable(writer, inputRows);
    writer.writeStartElement("DcConditions");
    const auto state = snapshot();
    for (int group = 0;
         group < qMax(dcNames.size(), qMax(dcRows.size(), qMax(dcRows2.size(), dcRows3.size()))); ++group) {
        writer.writeStartElement("Condition");
        writer.writeAttribute("name", dcNames.value(group));
        for (int source = 0; source < 3; ++source) {
            const auto row = state.dcSourceRows(source).value(group);
            writer.writeStartElement("Source");
            writer.writeAttribute("index", QString::number(source + 1));
            writer.writeTextElement("Vin", row.vin);
            writer.writeTextElement("CurrentLimit", row.currentLimit);
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    XmlWriter::writeRelayTable(writer, relayRows);
    XmlWriter::writeLoadTable(writer, loadMeta, loadRows);
    XmlWriter::writeDynamicTable(writer, dynamicMeta, dynamicRows);

    writer.writeEndElement(); // TestConditions
}

void TestConditionsModel::loadXml(QXmlStreamReader& reader)
{
    if (!reader.isStartElement() || reader.name() != QStringLiteral("TestConditions")) {
        reader.raiseError(QStringLiteral("Expected TestConditions element"));
        return;
    }
    if (reader.attributes().value("schemaVersion") != "2") {
        reader.raiseError("Unsupported TestConditions schema; create a new configuration.");
        return;
    }
    QVector<QString> dcNames;
    QSet<QString> seenTables;
    QVector<InputRow> inputRows;
    QVector<DcRow> dcRows;
    QVector<DcRow> dcRows2, dcRows3;
    QVector<RelayDataRow> relayRows;
    LoadMetaRow loadMeta;
    QVector<LoadDataRow> loadRows;
    DynamicMetaRow dynamicMeta;
    QVector<DynamicDataRow> dynamicRows;
    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isEndElement() && reader.name() == "TestConditions") {
            break;
        }

        if (reader.isStartElement()) {
            const QString table = reader.name().toString();
            if (seenTables.contains(table)) {
                reader.raiseError("Duplicate TestConditions table: " + table);
                return;
            }
            seenTables.insert(table);
            if (reader.name() == "InputTable") {
                XmlReader::readInputTable(reader, inputRows);
            } else if (reader.name() == "DcConditions") {
                while (reader.readNextStartElement()) {
                    if (reader.name() != "Condition") {
                        reader.raiseError("Expected DC Condition");
                        return;
                    }
                    dcNames.append(reader.attributes().value("name").toString());
                    DcGroup group;
                    bool seen[3] = {};
                    while (reader.readNextStartElement()) {
                        bool ok = false;
                        const int index = reader.attributes().value("index").toInt(&ok) - 1;
                        if (reader.name() != "Source" || !ok || index < 0 || index > 2 || seen[index]) {
                            reader.raiseError("DC condition requires unique Source indices 1..3");
                            return;
                        }
                        seen[index] = true;
                        bool voltage = false, current = false;
                        while (reader.readNextStartElement()) {
                            if (reader.name() == "Vin" && !voltage) {
                                group[index].vin = reader.readElementText();
                                voltage = true;
                            } else if (reader.name() == "CurrentLimit" && !current) {
                                group[index].currentLimit = reader.readElementText();
                                current = true;
                            } else {
                                reader.raiseError("Invalid DC Source field");
                                return;
                            }
                        }
                        if (!voltage || !current) {
                            reader.raiseError("Missing DC Source values");
                            return;
                        }
                    }
                    if (!seen[0] || !seen[1] || !seen[2]) {
                        reader.raiseError("Missing DC Source index");
                        return;
                    }
                    dcRows.append(group[0]);
                    dcRows2.append(group[1]);
                    dcRows3.append(group[2]);
                }
            } else if (reader.name() == "RelayTable") {
                XmlReader::readRelayTable(reader, relayRows);
            } else if (reader.name() == "LoadTable") {
                XmlReader::readLoadTable(reader, loadMeta, loadRows);
            } else if (reader.name() == "DynamicTable") {
                XmlReader::readDynamicTable(reader, dynamicMeta, dynamicRows);
            } else {
                reader.raiseError("Unexpected TestConditions element");
                return;
            }
        }
    }

    if (reader.hasError())
        return;
    if (seenTables != QSet<QString>{"InputTable", "DcConditions", "RelayTable", "LoadTable", "DynamicTable"}) {
        reader.raiseError("Incomplete TestConditions configuration");
        return;
    }
    this->inputRows = std::move(inputRows);
    this->dcNames = std::move(dcNames);
    this->dcRows = std::move(dcRows);
    this->dcRows2 = std::move(dcRows2);
    this->dcRows3 = std::move(dcRows3);
    this->relayRows = std::move(relayRows);
    this->loadMeta = std::move(loadMeta);
    this->loadRows = std::move(loadRows);
    this->dynamicMeta = std::move(dynamicMeta);
    this->dynamicRows = std::move(dynamicRows);
    emit configLoaded();
}

// XML 寫入器實現

void TestConditionsModel::XmlWriter::writeInputTable(QXmlStreamWriter& w, const QVector<InputRow>& rows)
{
    w.writeStartElement("InputTable");

    for (const auto& row : rows) {
        w.writeStartElement("Row");
        w.writeTextElement("PhaseMode", row.phaseMode.isEmpty() ? QStringLiteral("1phase") : row.phaseMode);
        w.writeTextElement("Vin", row.vin);
        w.writeTextElement("Frequency", row.frequency);
        w.writeTextElement("Phase", row.phase);
        w.writeEndElement(); // Row
    }

    w.writeEndElement(); // InputTable
}

void TestConditionsModel::XmlWriter::writeRelayTable(QXmlStreamWriter& w, const QVector<RelayDataRow>& rows)
{
    w.writeStartElement("RelayTable");
    w.writeStartElement("Rows");

    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }

    w.writeEndElement(); // Rows
    w.writeEndElement(); // RelayTable
}

void TestConditionsModel::XmlWriter::writeLoadTable(QXmlStreamWriter& w, const LoadMetaRow& meta,
                                           const QVector<LoadDataRow>& rows)
{
    w.writeStartElement("LoadTable");

    w.writeStartElement("Meta");
    writeStringVector(w, "Mode", meta.modes);
    writeStringVector(w, "Range", meta.ranges);
    writeStringVector(w, "Name", meta.names);
    writeStringVector(w, "Vo", meta.vo);
    writeStringVector(w, "Von", meta.von);
    w.writeEndElement(); // Meta

    w.writeStartElement("Rows");
    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }
    w.writeEndElement(); // Rows

    w.writeEndElement(); // LoadTable
}

void TestConditionsModel::XmlWriter::writeDynamicTable(QXmlStreamWriter& w, const DynamicMetaRow& meta,
                                              const QVector<DynamicDataRow>& rows)
{
    w.writeStartElement("DynamicTable");

    w.writeStartElement("Meta");
    writeStringVector(w, "Range", meta.ranges);
    writeStringVector(w, "Vo", meta.vo);
    writeStringVector(w, "Von", meta.von);
    writeStringVector(w, "T1T2", meta.t1t2);
    w.writeEndElement(); // Meta

    w.writeStartElement("Rows");
    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }
    w.writeEndElement(); // Rows

    w.writeEndElement(); // DynamicTable
}

void TestConditionsModel::XmlWriter::writeDataRow(QXmlStreamWriter& w, const QString& label,
                                         const QVector<QString>& values)
{
    w.writeStartElement("Row");
    w.writeAttribute("Label", label);

    for (const auto& v : values) {
        w.writeTextElement("Index", v);
    }

    w.writeEndElement(); // Row
}

void TestConditionsModel::XmlWriter::writeStringVector(QXmlStreamWriter& w, const QString& tag,
                                              const QVector<QString>& vec)
{
    w.writeStartElement(tag + "List");

    for (const auto& v : vec) {
        w.writeTextElement(tag, v);
    }

    w.writeEndElement(); // TagList
}

// XML 讀取器實現

void TestConditionsModel::XmlReader::readInputTable(QXmlStreamReader& r, QVector<InputRow>& rows)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "InputTable") {
            break;
        }

        if (r.isStartElement() && r.name() == "Row") {
            rows.append(readInputRow(r));
        }
    }
}

void TestConditionsModel::XmlReader::readRelayTable(QXmlStreamReader& r, QVector<RelayDataRow>& rows)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "RelayTable") {
            break;
        }

        if (r.isStartElement() && r.name() == "Row") {
            rows.append(readRelayDataRow(r));
        }
    }
}

void TestConditionsModel::XmlReader::readLoadTable(QXmlStreamReader& r, LoadMetaRow& meta, QVector<LoadDataRow>& rows)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "LoadTable") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "Meta") {
                readLoadMeta(r, meta);
            } else if (r.name() == "Row") {
                rows.append(readLoadDataRow(r));
            }
        }
    }
}

void TestConditionsModel::XmlReader::readDynamicTable(QXmlStreamReader& r, DynamicMetaRow& meta,
                                             QVector<DynamicDataRow>& rows)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "DynamicTable") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "Meta") {
                readDynamicMeta(r, meta);
            } else if (r.name() == "Row") {
                rows.append(readDynamicDataRow(r));
            }
        }
    }
}

// 行讀取輔助函數

InputRow TestConditionsModel::XmlReader::readInputRow(QXmlStreamReader& r)
{
    InputRow row;

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Row") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "PhaseMode") {
                row.phaseMode = r.readElementText();
            } else if (r.name() == "Vin") {
                row.vin = r.readElementText();
            } else if (r.name() == "Frequency") {
                row.frequency = r.readElementText();
            } else if (r.name() == "Phase") {
                row.phase = r.readElementText();
            }
        }
    }

    return row;
}

RelayDataRow TestConditionsModel::XmlReader::readRelayDataRow(QXmlStreamReader& r)
{
    RelayDataRow row;
    row.label = r.attributes().value("Label").toString();

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Row") {
            break;
        }

        if (r.isStartElement() && r.name() == "Index") {
            row.values << r.readElementText();
        }
    }

    return row;
}

LoadDataRow TestConditionsModel::XmlReader::readLoadDataRow(QXmlStreamReader& r)
{
    LoadDataRow row;
    row.label = r.attributes().value("Label").toString();

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Row") {
            break;
        }

        if (r.isStartElement() && r.name() == "Index") {
            row.values << r.readElementText();
        }
    }

    return row;
}

DynamicDataRow TestConditionsModel::XmlReader::readDynamicDataRow(QXmlStreamReader& r)
{
    DynamicDataRow row;
    row.label = r.attributes().value("Label").toString();

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Row") {
            break;
        }

        if (r.isStartElement() && r.name() == "Index") {
            row.values << r.readElementText();
        }
    }

    return row;
}

// Meta 讀取輔助函數

void TestConditionsModel::XmlReader::readLoadMeta(QXmlStreamReader& r, LoadMetaRow& meta)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Meta") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "ModeList") {
                meta.modes = readStringVector(r, "Mode");
            } else if (r.name() == "RangeList") {
                meta.ranges = readStringVector(r, "Range");
            } else if (r.name() == "NameList") {
                meta.names = readStringVector(r, "Name");
            } else if (r.name() == "VoList") {
                meta.vo = readStringVector(r, "Vo");
            } else if (r.name() == "VonList") {
                meta.von = readStringVector(r, "Von");
            }
        }
    }
}

void TestConditionsModel::XmlReader::readDynamicMeta(QXmlStreamReader& r, DynamicMetaRow& meta)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Meta") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "RangeList") {
                meta.ranges = readStringVector(r, "Range");
            } else if (r.name() == "VoList") {
                meta.vo = readStringVector(r, "Vo");
            } else if (r.name() == "VonList") {
                meta.von = readStringVector(r, "Von");
            } else if (r.name() == "T1T2List") {
                meta.t1t2 = readStringVector(r, "T1T2");
            }
        }
    }
}

QVector<QString> TestConditionsModel::XmlReader::readStringVector(QXmlStreamReader& r, const QString& tag)
{
    QVector<QString> result;
    QString listTag = tag + "List";

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == listTag) {
            break;
        }

        if (r.isStartElement() && r.name() == tag) {
            result << r.readElementText();
        }
    }

    return result;
}

void TestConditionsModel::XmlReader::skipToEndElement(QXmlStreamReader& r, const QString& elementName)
{
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == elementName) {
            break;
        }
    }
}
