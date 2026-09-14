#include "page3model.h"

Page3Model::Page3Model(QObject* parent)
    : QObject(parent)
{}

// ========== XML 寫入 ==========

void Page3Model::writeXml(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("Page3");

    writeComboBoxTitles(writer);
    writeCurrentSelections(writer);
    writeLoadData(writer);
    writeDynamicData(writer);
    writeRelayData(writer);

    writer.writeEndElement(); // Page3
}

void Page3Model::writeComboBoxTitles(QXmlStreamWriter& w) const
{
    w.writeStartElement("ComboBoxTitles");
    writeTitleList(w, "InputTitles", m_state.m_inputTitles);
    writeTitleList(w, "LoadTitles", m_state.m_loadTitles);
    writeTitleList(w, "DyLoadTitles", m_state.m_dyloadTitles);
    writeTitleList(w, "RelayTitles", m_state.m_relayTitles);
    w.writeEndElement();
}

void Page3Model::writeCurrentSelections(QXmlStreamWriter& w) const
{
    w.writeStartElement("CurrentSelections");

    auto writeSelection = [&](const QString& tag, int idx, const QString& txt) {
        w.writeStartElement(tag);
        w.writeAttribute("index", QString::number(idx));
        w.writeAttribute("text", txt);
        w.writeEndElement();
    };

    writeSelection("InputSelection", m_state.m_selectedInputIndex, m_state.m_selectedInputText);
    writeSelection("LoadSelection", m_state.m_selectedLoadIndex, m_state.m_selectedLoadText);
    writeSelection("DyLoadSelection", m_state.m_selectedDyLoadIndex, m_state.m_selectedDyLoadText);
    writeSelection("RelaySelection", m_state.m_selectedRelayIndex, m_state.m_selectedRelayText);

    w.writeEndElement();
}

void Page3Model::writeLoadData(QXmlStreamWriter& w) const
{
    // Meta
    w.writeStartElement("LoadMetaData");
    writeStringList(w, "Modes", "Mode", m_state.m_LoadMetaData.modes);
    writeStringList(w, "Ranges", "Range", m_state.m_LoadMetaData.ranges);
    writeStringList(w, "Names", "Name", m_state.m_LoadMetaData.names);
    writeStringList(w, "Vo", "Value", m_state.m_LoadMetaData.vo);
    writeStringList(w, "Von", "Value", m_state.m_LoadMetaData.von);
    w.writeEndElement();

    // Rows
    w.writeStartElement("LoadRowsData");
    for (const auto& row : m_state.m_LoadRowsData) {
        w.writeStartElement("LoadRow");
        w.writeAttribute("label", row.label);
        writeStringList(w, "Values", "Value", row.values);
        w.writeEndElement();
    }
    w.writeEndElement();
}

void Page3Model::writeDynamicData(QXmlStreamWriter& w) const
{
    // Meta
    w.writeStartElement("DynamicMetaData");
    writeStringList(w, "Ranges", "Range", m_state.m_DynamicMetaData.ranges);
    writeStringList(w, "Vo", "Value", m_state.m_DynamicMetaData.vo);
    writeStringList(w, "Von", "Value", m_state.m_DynamicMetaData.von);
    writeStringList(w, "T1T2", "Value", m_state.m_DynamicMetaData.t1t2);
    w.writeEndElement();

    // Rows
    w.writeStartElement("DynamicRowsData");
    for (const auto& row : m_state.m_DynamicRowsData) {
        w.writeStartElement("DynamicRow");
        w.writeAttribute("label", row.label);
        writeStringList(w, "Values", "Value", row.values);
        w.writeEndElement();
    }
    w.writeEndElement();
}

// ========== XML 讀取 ==========

void Page3Model::loadXml(QXmlStreamReader& reader)
{
    if (!reader.isStartElement() || reader.name() != QStringLiteral("Page3")) {
        reader.raiseError(QStringLiteral("Expected Page3 element"));
        return;
    }
    Page3Model candidate;
    // Preserve legacy partial-document behavior for omitted fields.
    candidate.m_state = m_state;
    candidate.readXmlFields(reader);
    if (!reader.hasError()) m_state = std::move(candidate.m_state);
}
void Page3Model::readXmlFields(QXmlStreamReader& reader)
{
    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isEndElement() && reader.name() == "Page3") {
            break;
        }

        if (reader.isStartElement()) {
            if (reader.name() == "ComboBoxTitles") {
                readComboBoxTitles(reader);
            }
            else if (reader.name() == "CurrentSelections") {
                readCurrentSelections(reader);
            }
            else if (reader.name() == "LoadMetaData") {
                readLoadMetaData(reader);
            }
            else if (reader.name() == "LoadRowsData") {
                readLoadRowsData(reader);
            }
            else if (reader.name() == "DynamicMetaData") {
                readDynamicMetaData(reader);
            }
            else if (reader.name() == "DynamicRowsData") {
                readDynamicRowsData(reader);
            }
            else if (reader.name() == "RelayRowsData") {
                readRelayData(reader);
            }
        }
    }
}

void Page3Model::readComboBoxTitles(QXmlStreamReader& r)
{
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "ComboBoxTitles") break;

        if (r.isStartElement()) {
            if (r.name() == "InputTitles") {
                m_state.m_inputTitles = readTitleList(r, "InputTitles");
            }
            else if (r.name() == "LoadTitles") {
                m_state.m_loadTitles = readTitleList(r, "LoadTitles");
            }
            else if (r.name() == "DyLoadTitles") {
                m_state.m_dyloadTitles = readTitleList(r, "DyLoadTitles");
            }
            else if (r.name() == "RelayTitles") {
                m_state.m_relayTitles = readTitleList(r, "RelayTitles");
            }
        }
    }
}

void Page3Model::readCurrentSelections(QXmlStreamReader& r)
{
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "CurrentSelections") break;

        if (r.isStartElement()) {
            QXmlStreamAttributes attrs = r.attributes();

            if (r.name() == "InputSelection") {
                m_state.m_selectedInputIndex = attrs.value("index").toInt();
                m_state.m_selectedInputText = attrs.value("text").toString();
            }
            else if (r.name() == "LoadSelection") {
                m_state.m_selectedLoadIndex = attrs.value("index").toInt();
                m_state.m_selectedLoadText = attrs.value("text").toString();
            }
            else if (r.name() == "DyLoadSelection") {
                m_state.m_selectedDyLoadIndex = attrs.value("index").toInt();
                m_state.m_selectedDyLoadText = attrs.value("text").toString();
            }
            else if (r.name() == "RelaySelection") {
                m_state.m_selectedRelayIndex = attrs.value("index").toInt();
                m_state.m_selectedRelayText = attrs.value("text").toString();
            }
            r.skipCurrentElement();
        }
    }
}

void Page3Model::readLoadMetaData(QXmlStreamReader& r)
{
    m_state.m_LoadMetaData = LoadMetaRow();

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "LoadMetaData") break;

        if (r.isStartElement()) {
            if (r.name() == "Modes") {
                m_state.m_LoadMetaData.modes = readStringList(r, "Modes", "Mode");
            }
            else if (r.name() == "Ranges") {
                m_state.m_LoadMetaData.ranges = readStringList(r, "Ranges", "Range");
            }
            else if (r.name() == "Names") {
                m_state.m_LoadMetaData.names = readStringList(r, "Names", "Name");
            }
            else if (r.name() == "Vo") {
                m_state.m_LoadMetaData.vo = readStringList(r, "Vo", "Value");
            }
            else if (r.name() == "Von") {
                m_state.m_LoadMetaData.von = readStringList(r, "Von", "Value");
            }
        }
    }
}

void Page3Model::readLoadRowsData(QXmlStreamReader& r)
{
    m_state.m_LoadRowsData.clear();

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "LoadRowsData") break;

        if (r.isStartElement() && r.name() == "LoadRow") {
            LoadDataRow row;
            row.label = r.attributes().value("label").toString();

            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && r.name() == "LoadRow") break;

                if (r.isStartElement() && r.name() == "Values") {
                    row.values = readStringList(r, "Values", "Value");
                }
            }
            m_state.m_LoadRowsData << row;
        }
    }
}

void Page3Model::readDynamicMetaData(QXmlStreamReader& r)
{
    m_state.m_DynamicMetaData = DynamicMetaRow();

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "DynamicMetaData") break;

        if (r.isStartElement()) {
            if (r.name() == "Ranges") {
                m_state.m_DynamicMetaData.ranges = readStringList(r, "Ranges", "Range");
            }
            if (r.name() == "Vo") {
                m_state.m_DynamicMetaData.vo = readStringList(r, "Vo", "Value");
            }
            if (r.name() == "Von") {
                m_state.m_DynamicMetaData.von = readStringList(r, "Von", "Value");
            }
            if (r.name() == "T1T2") {
                m_state.m_DynamicMetaData.t1t2 = readStringList(r, "T1T2", "Value");
            }
        }
    }
}

void Page3Model::readDynamicRowsData(QXmlStreamReader& r)
{
    m_state.m_DynamicRowsData.clear();

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "DynamicRowsData") break;

        if (r.isStartElement() && r.name() == "DynamicRow") {
            DynamicDataRow row;
            row.label = r.attributes().value("label").toString();

            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && r.name() == "DynamicRow") break;

                if (r.isStartElement() && r.name() == "Values") {
                    row.values = readStringList(r, "Values", "Value");
                }
            }
            m_state.m_DynamicRowsData << row;
        }
    }
}

// ========== 輔助函數 ==========

void Page3Model::writeTitleList(QXmlStreamWriter& w, const QString& tag, const QStringList& list)
{
    w.writeStartElement(tag);
    for (const auto& title : list) {
        w.writeTextElement("Title", title);
    }
    w.writeEndElement();
}

void Page3Model::writeStringList(QXmlStreamWriter& w, const QString& parentTag,
                                 const QString& itemTag, const QVector<QString>& list)
{
    w.writeStartElement(parentTag);
    for (const auto& item : list) {
        w.writeTextElement(itemTag, item);
    }
    w.writeEndElement();
}

QStringList Page3Model::readTitleList(QXmlStreamReader& r, const QString& endTag)
{
    QStringList result;

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == endTag) break;

        if (r.isStartElement() && r.name() == "Title") {
            result << r.readElementText();
        }
    }

    return result;
}

QVector<QString> Page3Model::readStringList(QXmlStreamReader& r, const QString& endTag,
                                            const QString& itemTag)
{
    QVector<QString> result;

    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == endTag) break;

        if (r.isStartElement() && r.name() == itemTag) {
            result << r.readElementText();
        }
    }

    return result;
}

void Page3Model::writeRelayData(QXmlStreamWriter& w) const
{
    w.writeStartElement("RelayRowsData");
    for (const auto& row : m_state.m_RelayRowsData) {
        w.writeStartElement("Row");
        w.writeAttribute("label", row.label);
        // 使用現有的工具函數寫入數值列表
        writeStringList(w, "Values", "Val", row.values);
        w.writeEndElement(); // Row
    }
    w.writeEndElement(); // RelayRowsData
}

void Page3Model::readRelayData(QXmlStreamReader& r)
{
    m_state.m_RelayRowsData.clear();
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == "RelayRowsData") break;

        if (r.isStartElement() && r.name() == "Row") {
            RelayDataRow row;
            row.label = r.attributes().value("label").toString();
            // 讀取內部列表
            while(!r.atEnd()) {
                r.readNext();
                if(r.isStartElement() && r.name() == "Values") {
                    row.values = readStringList(r, "Values", "Val");
                }
                if(r.isEndElement() && r.name() == "Row") break;
            }
            m_state.m_RelayRowsData.append(row);
        }
    }
}
