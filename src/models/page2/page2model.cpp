#include "page2model.h"
#include <QDebug>

Page2Model::Page2Model(QObject *parent)
    : QObject(parent)
{}

// ========== 主要 XML 操作 ==========

void Page2Model::writeXml(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("Page2");

    XmlWriter::writeInputTable(writer, inputRows);
    XmlWriter::writeRelayTable(writer, relayRows);
    XmlWriter::writeLoadTable(writer, loadMeta, loadRows);
    XmlWriter::writeDynamicTable(writer, dynamicMeta, dynamicRows);

    writer.writeEndElement(); // Page2
}

void Page2Model::loadXml(QXmlStreamReader& reader)
{
    inputRows.clear();
    relayRows.clear();
    loadMeta = {};
    loadRows.clear();
    dynamicMeta = {};
    dynamicRows.clear();

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isEndElement() && reader.name() == "Page2") {
            break;
        }

        if (reader.isStartElement()) {
            if (reader.name() == "InputTable") {
                XmlReader::readInputTable(reader, inputRows);
            }
            else if (reader.name() == "RelayTable") {
                XmlReader::readRelayTable(reader, relayRows);
            }
            else if (reader.name() == "LoadTable") {
                XmlReader::readLoadTable(reader, loadMeta, loadRows);
            }
            else if (reader.name() == "DynamicTable") {
                XmlReader::readDynamicTable(reader, dynamicMeta, dynamicRows);
            }
        }
    }

    emit configLoaded();
}

// ========== XML 寫入器實現 ==========

void Page2Model::XmlWriter::writeInputTable(QXmlStreamWriter& w, const QVector<InputRow>& rows)
{
    w.writeStartElement("InputTable");

    for (const auto& row : rows) {
        w.writeStartElement("Row");
        w.writeTextElement("Vin", row.vin);
        w.writeTextElement("Frequency", row.frequency);
        w.writeTextElement("Phase", row.phase);
        w.writeEndElement(); // Row
    }

    w.writeEndElement(); // InputTable
}

void Page2Model::XmlWriter::writeRelayTable(QXmlStreamWriter& w, const QVector<RelayDataRow>& rows)
{
    w.writeStartElement("RelayTable");
    w.writeStartElement("Rows");

    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }

    w.writeEndElement(); // Rows
    w.writeEndElement(); // RelayTable
}

void Page2Model::XmlWriter::writeLoadTable(QXmlStreamWriter& w, const LoadMetaRow& meta,
                                           const QVector<LoadDataRow>& rows)
{
    w.writeStartElement("LoadTable");

    // Meta
    w.writeStartElement("Meta");
    writeStringVector(w, "Mode", meta.modes);
    writeStringVector(w, "Name", meta.names);
    writeStringVector(w, "Vo", meta.vo);
    writeStringVector(w, "Von", meta.von);
    writeStringVector(w, "RiseSlopeCCH", meta.riseSlopeCCH);
    writeStringVector(w, "FallSlopeCCH", meta.fallSlopeCCH);
    writeStringVector(w, "RiseSlopeCCL", meta.riseSlopeCCL);
    writeStringVector(w, "FallSlopeCCL", meta.fallSlopeCCL);
    w.writeEndElement(); // Meta

    // Rows
    w.writeStartElement("Rows");
    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }
    w.writeEndElement(); // Rows

    w.writeEndElement(); // LoadTable
}

void Page2Model::XmlWriter::writeDynamicTable(QXmlStreamWriter& w, const DynamicMetaRow& meta,
                                              const QVector<DynamicDataRow>& rows)
{
    w.writeStartElement("DynamicTable");

    // Meta
    w.writeStartElement("Meta");
    writeStringVector(w, "Vo", meta.vo);
    writeStringVector(w, "Von", meta.von);
    writeStringVector(w, "RiseSlopeCCDH", meta.riseSlopeCCDH);
    writeStringVector(w, "FallSlopeCCDH", meta.fallSlopeCCDH);
    writeStringVector(w, "RiseSlopeCCDL", meta.riseSlopeCCDL);
    writeStringVector(w, "FallSlopeCCDL", meta.fallSlopeCCDL);
    writeStringVector(w, "T1T2", meta.t1t2);
    w.writeEndElement(); // Meta

    // Rows
    w.writeStartElement("Rows");
    for (const auto& row : rows) {
        writeDataRow(w, row.label, row.values);
    }
    w.writeEndElement(); // Rows

    w.writeEndElement(); // DynamicTable
}

void Page2Model::XmlWriter::writeDataRow(QXmlStreamWriter& w, const QString& label,
                                         const QVector<QString>& values)
{
    w.writeStartElement("Row");
    w.writeAttribute("Label", label);

    for (const auto& v : values) {
        w.writeTextElement("Index", v);
    }

    w.writeEndElement(); // Row
}

void Page2Model::XmlWriter::writeStringVector(QXmlStreamWriter& w, const QString& tag,
                                              const QVector<QString>& vec)
{
    w.writeStartElement(tag + "List");

    for (const auto& v : vec) {
        w.writeTextElement(tag, v);
    }

    w.writeEndElement(); // TagList
}

// ========== XML 讀取器實現 ==========

void Page2Model::XmlReader::readInputTable(QXmlStreamReader& r, QVector<InputRow>& rows)
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

void Page2Model::XmlReader::readRelayTable(QXmlStreamReader& r, QVector<RelayDataRow>& rows)
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

void Page2Model::XmlReader::readLoadTable(QXmlStreamReader& r, LoadMetaRow& meta,
                                          QVector<LoadDataRow>& rows)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "LoadTable") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "Meta") {
                readLoadMeta(r, meta);
            }
            else if (r.name() == "Row") {
                rows.append(readLoadDataRow(r));
            }
        }
    }
}

void Page2Model::XmlReader::readDynamicTable(QXmlStreamReader& r, DynamicMetaRow& meta,
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
            }
            else if (r.name() == "Row") {
                rows.append(readDynamicDataRow(r));
            }
        }
    }
}

// ========== 行讀取輔助函數 ==========

InputRow Page2Model::XmlReader::readInputRow(QXmlStreamReader& r)
{
    InputRow row;

    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Row") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "Vin") {
                row.vin = r.readElementText();
            }
            else if (r.name() == "Frequency") {
                row.frequency = r.readElementText();
            }
            else if (r.name() == "Phase") {
                row.phase = r.readElementText();
            }
        }
    }

    return row;
}

RelayDataRow Page2Model::XmlReader::readRelayDataRow(QXmlStreamReader& r)
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

LoadDataRow Page2Model::XmlReader::readLoadDataRow(QXmlStreamReader& r)
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

DynamicDataRow Page2Model::XmlReader::readDynamicDataRow(QXmlStreamReader& r)
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

// ========== Meta 讀取輔助函數 ==========

void Page2Model::XmlReader::readLoadMeta(QXmlStreamReader& r, LoadMetaRow& meta)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Meta") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "ModeList") {
                meta.modes = readStringVector(r, "Mode");
            }
            else if (r.name() == "NameList") {
                meta.names = readStringVector(r, "Name");
            }
            else if (r.name() == "VoList") {
                meta.vo = readStringVector(r, "Vo");
            }
            else if (r.name() == "VonList") {
                meta.von = readStringVector(r, "Von");
            }
            else if (r.name() == "RiseSlopeCCHList") {
                meta.riseSlopeCCH = readStringVector(r, "RiseSlopeCCH");
            }
            else if (r.name() == "FallSlopeCCHList") {
                meta.fallSlopeCCH = readStringVector(r, "FallSlopeCCH");
            }
            else if (r.name() == "RiseSlopeCCLList") {
                meta.riseSlopeCCL = readStringVector(r, "RiseSlopeCCL");
            }
            else if (r.name() == "FallSlopeCCLList") {
                meta.fallSlopeCCL = readStringVector(r, "FallSlopeCCL");
            }
        }
    }
}

void Page2Model::XmlReader::readDynamicMeta(QXmlStreamReader& r, DynamicMetaRow& meta)
{
    while (!r.atEnd()) {
        r.readNext();

        if (r.isEndElement() && r.name() == "Meta") {
            break;
        }

        if (r.isStartElement()) {
            if (r.name() == "VoList") {
                meta.vo = readStringVector(r, "Vo");
            }
            else if (r.name() == "VonList") {
                meta.von = readStringVector(r, "Von");
            }
            else if (r.name() == "RiseSlopeCCDHList") {
                meta.riseSlopeCCDH = readStringVector(r, "RiseSlopeCCDH");
            }
            else if (r.name() == "FallSlopeCCDHList") {
                meta.fallSlopeCCDH = readStringVector(r, "FallSlopeCCDH");
            }
            else if (r.name() == "RiseSlopeCCDLList") {
                meta.riseSlopeCCDL = readStringVector(r, "RiseSlopeCCDL");
            }
            else if (r.name() == "FallSlopeCCDLList") {
                meta.fallSlopeCCDL = readStringVector(r, "FallSlopeCCDL");
            }
            else if (r.name() == "T1T2List") {
                meta.t1t2 = readStringVector(r, "T1T2");
            }
        }
    }
}

QVector<QString> Page2Model::XmlReader::readStringVector(QXmlStreamReader& r, const QString& tag)
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

void Page2Model::XmlReader::skipToEndElement(QXmlStreamReader& r, const QString& elementName)
{
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == elementName) {
            break;
        }
    }
}
