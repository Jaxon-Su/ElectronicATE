#include "xmlconfigstore.h"
#include <QFile>
#include <QMap>
#include <QSet>
#include <QSignalBlocker>
#include <QPointer>
#include <QScopeGuard>
#include <memory>
#include <vector>
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
    const QString& fileName, const QList<IXmlSerializable*>& pages,
    const std::function<void()>& synchronize)
{
    static thread_local bool loading = false;
    if (loading) return {XmlOperationResult::Error::Parse, QStringLiteral("A configuration load is already in progress")};
    loading = true;
    const auto resetLoading = qScopeGuard([] { loading = false; });
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

    QSet<IXmlSerializable*> touched;
    const auto visitPages = [&](bool validateOnly) -> XmlOperationResult {
        QXmlStreamReader reader(document);
        reader.readNextStartElement(); // validated root
        QSet<QString> seen;
        while (reader.readNextStartElement()) {
            if (auto* page = pageMap.value(reader.name().toString(), nullptr)) {
                const QString tag = reader.name().toString();
                if (seen.contains(tag)) { reader.raiseError("Duplicate page section: " + tag); break; }
                seen.insert(tag);
                touched.insert(page);
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
    // Suppress outward ViewModel notifications while all live states are replaced.
    // Model-to-ViewModel updates still run, keeping each page cache in sync.
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    QMap<IXmlSerializable*, QByteArray> backups;
    QMap<IXmlSerializable*, QPointer<QObject>> owners;
    for (auto* page : pages) {
        if (!page) continue;
        QByteArray snapshot;
        QXmlStreamWriter writer(&snapshot);
        page->writeXml(writer);
        if (writer.hasError()) return {XmlOperationResult::Error::Write, QStringLiteral("Cannot snapshot current configuration")};
        backups.insert(page, snapshot);
        if (auto* object = dynamic_cast<QObject*>(page)) {
            owners.insert(page, object);
            blockers.push_back(std::make_unique<QSignalBlocker>(object));
        }
    }
    XmlOperationResult applied;
    try {
        applied = visitPages(false);
        if (applied.succeeded() && !touched.isEmpty() && synchronize) synchronize();
    }
    catch (const std::exception& error) { applied = {XmlOperationResult::Error::Parse, QString::fromUtf8(error.what())}; }
    catch (...) { applied = {XmlOperationResult::Error::Parse, QStringLiteral("Configuration apply failed")}; }
    if (!applied.succeeded()) {
        for (auto* page : pages) {
            if (!page) continue;
            QXmlStreamReader restore(backups.value(page));
            restore.readNextStartElement();
            try { page->loadXml(restore); }
            catch (...) { restore.raiseError("Rollback failed"); }
            if (restore.hasError()) applied.detail += QStringLiteral("; rollback failed for ") + page->xmlTagName();
        }
        if (synchronize) {
            try { synchronize(); }
            catch (...) { applied.detail += QStringLiteral("; dependency rollback failed"); }
        }
        return applied;
    }
    blockers.clear();
    // Caller order is the dependency order, independent of XML element order.
    for (auto* page : pages) {
        if (!touched.contains(page)) continue;
        if (owners.contains(page) && owners.value(page).isNull())
            return {XmlOperationResult::Error::Parse, QStringLiteral("Configuration owner closed during notification")};
        page->publishXmlLoaded();
    }
    return {};
}
