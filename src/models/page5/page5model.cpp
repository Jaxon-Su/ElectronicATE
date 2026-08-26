#include "page5model.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

Page5Model::Page5Model(QObject* parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────
//  writeXml
//  只儲存 Page5 自有的 DUT Test 表格
//  Input / Load / DyLoad / Relay 資料由 Page2 負責儲存，不重複寫入
// ─────────────────────────────────────────────
void Page5Model::writeXml(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("Page5");

    writer.writeStartElement("DutTable");
    for (const auto& row : dutRows) {
        writer.writeStartElement("Row");
        writer.writeAttribute("Active", row.active ? "1" : "0");
        writer.writeAttribute("Report", row.report ? "1" : "0");
        writer.writeTextElement("Item",  row.item);
        writer.writeTextElement("Ext",   row.ext);
        writer.writeTextElement("Retry", row.retry);
        if (!row.settings.isEmpty()) {
            const QByteArray json =
                QJsonDocument::fromVariant(row.settings).toJson(QJsonDocument::Compact);
            writer.writeTextElement("Settings", QString::fromUtf8(json));
        }
        writer.writeEndElement(); // Row
    }
    writer.writeEndElement(); // DutTable

    writer.writeEndElement(); // Page5
}

// ─────────────────────────────────────────────
//  loadXml
// ─────────────────────────────────────────────
void Page5Model::loadXml(QXmlStreamReader& reader)
{
    dutRows.clear();

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isEndElement() && reader.name() == QLatin1String("Page5"))
            break;

        if (!reader.isStartElement()) continue;

        if (reader.name() == QLatin1String("DutTable"))
            readDutTable(reader);
    }

    emit configLoaded();
}

// ─────────────────────────────────────────────
//  readDutTable
// ─────────────────────────────────────────────
void Page5Model::readDutTable(QXmlStreamReader& r)
{
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == QLatin1String("DutTable")) break;
        if (!r.isStartElement() || r.name() != QLatin1String("Row")) continue;

        DutRowData row;
        row.active = (r.attributes().value("Active").toString() == "1");
        row.report = (r.attributes().value("Report").toString() == "1");

        while (!r.atEnd()) {
            r.readNext();
            if (r.isEndElement() && r.name() == QLatin1String("Row")) break;
            if (!r.isStartElement()) continue;
            const QString n = r.name().toString();
            if      (n == "Item")  row.item  = r.readElementText();
            else if (n == "Ext")   row.ext   = r.readElementText();
            else if (n == "Retry") row.retry = r.readElementText();
            else if (n == "Settings") {
                const QByteArray json = r.readElementText().toUtf8();
                row.settings = QJsonDocument::fromJson(json).toVariant().toMap();
            }
        }
        dutRows.append(row);
    }
}
