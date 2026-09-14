#include "xmlconfigstore.h"
#include <QFile>
#include <QMap>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

XmlOperationResult XmlConfigStore::saveAllToXml(
    const QString& fileName, const QList<IXmlSerializable*>& pages)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {XmlOperationResult::Error::Open, file.errorString()};

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);
    writer.writeStartDocument();
    writer.writeStartElement("loodGUI");
    for (auto* page : pages) {
        if (page) page->writeXml(writer);
    }
    writer.writeEndElement();
    writer.writeEndDocument();

    if (writer.hasError()) {
        file.cancelWriting();
        return {XmlOperationResult::Error::Write, QStringLiteral("XML serialization failed")};
    }
    if (!file.commit())
        return {XmlOperationResult::Error::Write, file.errorString()};
    return {};
}

XmlOperationResult XmlConfigStore::loadAllFromXml(
    const QString& fileName, const QList<IXmlSerializable*>& pages)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {XmlOperationResult::Error::Open, file.errorString()};

    // Validate the complete snapshot before letting page loaders mutate models.
    const QByteArray document = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return {XmlOperationResult::Error::Open, file.errorString()};
    QXmlStreamReader validation(document);
    if (!validation.readNextStartElement())
        return {XmlOperationResult::Error::Parse, validation.errorString(), validation.lineNumber()};
    if (validation.name() != QStringLiteral("loodGUI"))
        return {XmlOperationResult::Error::InvalidRoot, validation.name().toString(), validation.lineNumber()};
    while (!validation.atEnd()) validation.readNext();
    if (validation.hasError())
        return {XmlOperationResult::Error::Parse, validation.errorString(), validation.lineNumber()};

    QMap<QString, IXmlSerializable*> pageMap;
    for (auto* page : pages) {
        if (page) pageMap[page->xmlTagName()] = page;
    }

    const auto visitPages = [&](bool validateOnly) -> XmlOperationResult {
        QXmlStreamReader reader(document);
        reader.readNextStartElement(); // validated root
        while (reader.readNextStartElement()) {
            if (auto* page = pageMap.value(reader.name().toString(), nullptr)) {
                if (validateOnly) page->validateXml(reader);
                else page->loadXml(reader);
            } else {
                reader.skipCurrentElement();
            }
        }
        if (reader.hasError())
            return {XmlOperationResult::Error::Parse, reader.errorString(), reader.lineNumber()};
        return {};
    };
    // Page-specific parsing must also succeed before any live model or UI is updated.
    const auto result = visitPages(true);
    if (!result.succeeded()) return result;
    return visitPages(false);
}
