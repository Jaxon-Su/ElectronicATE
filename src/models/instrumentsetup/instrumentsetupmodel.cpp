#include "instrumentsetupmodel.h"
#include "channelnumberpolicy.h"
#include "chroma6310frame.h"
#include "chroma63600frame.h"
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include <QSet>

namespace {
QString frameMappingError(const InstrumentSetupConfig &config)
{
    for (const auto &load : config.instruments) {
        if (load.type != "Load" || !Chroma63600Frame::lastChannel(load.modelName) ||
            load.getResourceString().isEmpty() || load.channelNumbers.isEmpty()) continue;
        for (int slot = 0; slot < load.channels.size(); ++slot) {
            if (load.channels[slot].index <= 0 || load.channels[slot].subModel.isEmpty()) continue;
            const auto error = Chroma63600Frame::mappingError(config, load, slot);
            if (!error.isEmpty()) return error;
        }
    }
    return {};
}
}

InstrumentSetupModel::InstrumentSetupModel(QObject* parent) : QObject(parent)
{
}

// Public Methods

void InstrumentSetupModel::setInstrumentConfigs(const QList<InstrumentConfig>& configs)
{
    m_config.instruments = configs;
}

bool InstrumentSetupModel::loadBaseXml(const QString& fileName)
{
    QDomDocument doc;
    if (!openAndParseXmlFile(fileName, doc)) {
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "root") {
        return false;
    }

    clearAllMaps();

    QDomElement instsElem = root.firstChildElement("instruments");
    QDomNodeList nodes = instsElem.elementsByTagName("instrument");

    collectTemplates(nodes);
    processInstruments(nodes);

    m_config.loadOutputs = 1;
    m_config.relayOutputs = 1;
    m_config.dcInputs = 1;
    emit configLoaded(m_config);

    return true;
}

void InstrumentSetupModel::writeXml(QXmlStreamWriter& writer) const
{
    const auto error = frameMappingError(m_config);
    if (!error.isEmpty()) {
        writer.raiseError(error);
        return;
    }
    writer.writeStartElement("InstrumentSetup");
    // Older applications interpret physical CH by list position. Prevent them from
    // loading a numbered layout as if the added CH6/8 were the old CH7/9.
    const bool numbered = std::any_of(m_config.instruments.cbegin(), m_config.instruments.cend(),
        [](const InstrumentConfig &ic) { return !ic.channelNumbers.isEmpty() && ic.channelNumbers.first() > 0; });
    writer.writeAttribute("schemaVersion", numbered ? "3" : "2");
    writer.writeTextElement("LoadOutputs", QString::number(m_config.loadOutputs));
    writer.writeTextElement("RelayOutputs", QString::number(m_config.relayOutputs));
    writer.writeTextElement("DcInputs", QString::number(m_config.dcInputs));
    writeInstruments(writer);
    writer.writeEndElement(); // InstrumentSetup
}

QList<int> InstrumentSetupModel::catalogChannels(bool legacy) const
{
    QSet<int> all;
    for (auto it = channelsMap.cbegin(); it != channelsMap.cend(); ++it) {
        auto numbers = ChannelNumberPolicy::parse(it.value());
        if (legacy && Chroma6310Frame::isFrame(it.key()))
            numbers.intersect(QSet<int>{1, 3, 5, 7});
        if (legacy && Chroma63600Frame::lastChannel(it.key()) && it.key() != "63601-5")
            numbers.intersect(QSet<int>{1, 3, 5, 7, 9}); // Unnumbered XML predates the even CH columns.
        all.unite(numbers);
    }
    return ChannelNumberPolicy::sorted(all);
}

void InstrumentSetupModel::loadXml(QXmlStreamReader& reader, const InstrumentSetupModel* catalog)
{
    if (!reader.isStartElement() || reader.name() != QStringLiteral("InstrumentSetup")) {
        reader.raiseError(QStringLiteral("Expected InstrumentSetup element"));
        return;
    }
    const auto version = reader.attributes().value("schemaVersion").toString();
    if (version != "2" && version != "3") {
        reader.raiseError("Unsupported InstrumentSetup schema; create a new configuration.");
        return;
    }
    InstrumentSetupConfig config;
    QSet<QString> seen;

    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "InstrumentSetup")) {
        reader.readNext();

        if (!reader.isStartElement()) {
            continue;
        }

        const auto field = reader.name().toString();
        if (seen.contains(field)) {
            reader.raiseError("Duplicate InstrumentSetup field: " + field);
            return;
        }
        seen.insert(field);
        if (reader.name() == "LoadOutputs") {
            config.loadOutputs = reader.readElementText().toInt();
        } else if (reader.name() == "RelayOutputs") {
            config.relayOutputs = reader.readElementText().toInt();
        } else if (reader.name() == "DcInputs") {
            bool ok = false;
            config.dcInputs = reader.readElementText().toInt(&ok);
            if (!ok || config.dcInputs < 1 || config.dcInputs > 3) {
                reader.raiseError("DcInputs must be between 1 and 3");
                return;
            }
        } else if (reader.name() == "Instruments") {
            loadInstrumentsFromXml(reader, config, version == "3");
        } else {
            reader.raiseError("Unexpected InstrumentSetup field");
            return;
        }
    }

    if (reader.hasError())
        return;

    if (seen != QSet<QString>{"LoadOutputs", "RelayOutputs", "DcInputs", "Instruments"}) {
        reader.raiseError("Incomplete InstrumentSetup configuration");
        return;
    }
    if (config.loadOutputs < 1 || config.loadOutputs > 20 || config.relayOutputs < 1 ||
        config.relayOutputs > 20) {
        reader.raiseError("Invalid output count");
        return;
    }
    QSet<QString> dcSources;
    const auto &source = catalog ? *catalog : *this;
    const auto available = source.catalogChannels();
    const auto legacy = source.catalogChannels(true);
    for (auto &ic : config.instruments) {
        if (ic.channels.isEmpty() || available.isEmpty()) continue;
        if (ic.channelNumbers.isEmpty()) {
            if (ic.channels.size() > legacy.size()) {
                reader.raiseError("Cannot reconstruct physical channels from the legacy instrument catalog");
                return;
            }
            ic.channelNumbers = legacy.first(ic.channels.size());
        }
        for (int ch : ic.channelNumbers) {
            if (!available.contains(ch)) {
                reader.raiseError(QString("Physical CH%1 is missing from the current instrument catalog").arg(ch));
                return;
            }
        }
    }
    const auto error = frameMappingError(config);
    if (!error.isEmpty()) {
        reader.raiseError(error);
        return;
    }
    for (const auto& instrument : config.instruments) {
        if (instrument.type != "InputDCSource")
            continue;
        if (!QStringList{"DC Source1", "DC Source2", "DC Source3"}.contains(instrument.name) ||
            dcSources.contains(instrument.name)) {
            reader.raiseError("Invalid or duplicate DC Source name");
            return;
        }
        dcSources.insert(instrument.name);
    }
    m_config = config;
    emit configLoaded(m_config);
}

// Private Helper Methods for loadBaseXml

bool InstrumentSetupModel::openAndParseXmlFile(const QString& fileName, QDomDocument& doc)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }

    file.close();
    return true;
}

void InstrumentSetupModel::clearAllMaps()
{
    m_config.instruments.clear();
    xmlModelMap.clear();
    modelNames.clear();
    subModelMap.clear();
    channelsMap.clear();
    templateMap.clear();
}

void InstrumentSetupModel::collectTemplates(const QDomNodeList& nodes)
{
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement element = nodes.at(i).toElement();
        if (element.attribute("type") == "Template") {
            templateMap[element.attribute("name")] = element;
        }
    }
}

void InstrumentSetupModel::processInstruments(const QDomNodeList& nodes)
{
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement element = nodes.at(i).toElement();
        QString type = element.attribute("type");

        if (type == "Template") {
            continue;
        }

        InstrumentConfig ic = createInstrumentConfig(element);
        QList<QDomElement> models = getModelElements(element);

        processModels(models, type, ic.name);

        QStringList modelNamesList;
        for (const auto& modelElement : models) {
            modelNamesList << modelElement.firstChildElement("modelName").text();
        }
        xmlModelMap[ic.name] = modelNamesList;

        m_config.instruments.append(ic);
    }
}

InstrumentConfig InstrumentSetupModel::createInstrumentConfig(const QDomElement& element)
{
    InstrumentConfig ic;
    ic.name = element.attribute("name");
    ic.type = element.attribute("type");
    ic.modelName = "";
    ic.enabled = true;
    ic.channels.clear();
    return ic;
}

QList<QDomElement> InstrumentSetupModel::getModelElements(const QDomElement& element)
{
    QList<QDomElement> models;

    if (element.hasAttribute("ref")) {
        QString ref = element.attribute("ref");
        if (templateMap.contains(ref)) {
            QDomNodeList refModels = templateMap[ref].elementsByTagName("model");
            for (int i = 0; i < refModels.count(); ++i) {
                models.append(refModels.at(i).toElement());
            }
        }
    } else {
        QDomNodeList directModels = element.elementsByTagName("model");
        for (int i = 0; i < directModels.count(); ++i) {
            models.append(directModels.at(i).toElement());
        }
    }

    return models;
}

void InstrumentSetupModel::processModels(const QList<QDomElement>& models, const QString& type,
                               const QString& instrumentName)
{
    for (const auto& modelElement : models) {
        QString modelName = modelElement.firstChildElement("modelName").text();
        processModel(modelElement, type, modelName);
    }
}

void InstrumentSetupModel::processModel(const QDomElement& modelElement, const QString& type, const QString& modelName)
{
    // 更新 modelNames（僅對 "Load" 類型）
    if (type == "Load" && !modelNames.contains(modelName)) {
        modelNames.append(modelName);
    }

    // 提取並保存 subModels
    QStringList subModels = extractSubModels(modelElement);
    subModelMap[modelName] = subModels;

    // 提取並保存 channels
    QStringList channels = extractChannels(modelElement);
    channelsMap[modelName] = channels;
}

QStringList InstrumentSetupModel::extractSubModels(const QDomElement& modelElement)
{
    QStringList subModels;
    QDomNodeList subModelNodes = modelElement.elementsByTagName("subModel");

    for (int i = 0; i < subModelNodes.count(); ++i) {
        subModels << subModelNodes.at(i).toElement().attribute("name");
    }

    return subModels;
}

QStringList InstrumentSetupModel::extractChannels(const QDomElement& modelElement)
{
    QString channelsText = modelElement.firstChildElement("channels").text();
    return channelsText.split(",", Qt::SkipEmptyParts);
}

// Private Helper Methods for writeXml

void InstrumentSetupModel::writeInstruments(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("Instruments");
    for (const auto& ic : m_config.instruments) {
        writeInstrument(writer, ic);
    }
    writer.writeEndElement(); // Instruments
}

void InstrumentSetupModel::writeInstrument(QXmlStreamWriter& writer, const InstrumentConfig& ic) const
{
    writer.writeStartElement("Instrument");
    writer.writeAttribute("name", ic.name);
    writer.writeAttribute("type", ic.type);
    writer.writeAttribute("enabled", ic.enabled ? "true" : "false");
    writer.writeTextElement("ModelName", ic.modelName);

    ic.commConfig.writeXml(writer);

    writeChannels(writer, ic);
    writer.writeEndElement(); // Instrument
}

void InstrumentSetupModel::writeChannels(QXmlStreamWriter& writer, const InstrumentConfig& ic) const
{
    writer.writeStartElement("Channels");
    if (!ic.channels.isEmpty() && ic.channelNumbers.value(0) <= 0)
        writer.writeAttribute("layout", "legacy");
    for (int i = 0; i < ic.channels.size(); ++i) {
        const auto &ch = ic.channels[i];
        writer.writeStartElement("Channel");
        if (ic.channelNumbers.value(i) > 0)
            writer.writeAttribute("number", QString::number(ic.channelNumbers[i]));
        writer.writeAttribute("subModel", ch.subModel);
        writer.writeAttribute("index", QString::number(ch.index));
        if (ch.syncType >= 0 && ch.syncType <= 2) {
            static const char* syncTypeNames[] = {"NONE", "MASTER", "SLAVE"};
            writer.writeAttribute("syncType", syncTypeNames[ch.syncType]);
        }
        writer.writeEndElement(); // Channel
    }
    writer.writeEndElement(); // Channels
}

// Private Helper Methods for loadXml

void InstrumentSetupModel::loadInstrumentsFromXml(QXmlStreamReader& reader, InstrumentSetupConfig& config, bool numbered)
{
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Instruments")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Instrument") {
            InstrumentConfig ic = loadInstrumentFromXml(reader, numbered);
            config.instruments.append(ic);
        }
    }
}

InstrumentConfig InstrumentSetupModel::loadInstrumentFromXml(QXmlStreamReader& reader, bool numbered)
{
    InstrumentConfig ic;
    QSet<QString> fields;
    ic.name = reader.attributes().value("name").toString();
    ic.type = reader.attributes().value("type").toString();
    ic.enabled = (reader.attributes().value("enabled") == "true");

    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == QString("Instrument"))) {
        reader.readNext();

        if (!reader.isStartElement()) {
            continue;
        }

        QString elementName = reader.name().toString();
        if (fields.contains(elementName)) {
            reader.raiseError("Duplicate Instrument field: " + elementName);
            return ic;
        }
        fields.insert(elementName);

        if (elementName == "ModelName") {
            ic.modelName = reader.readElementText();
        } else if (elementName == "CommunicationConfig") {
            ic.commConfig.readXml(reader);
        } else if (elementName == "Channels") {
            loadChannelsFromXml(reader, ic, numbered);
        } else {
            reader.raiseError("Unexpected Instrument field: " + elementName);
            return ic;
        }
    }

    if (!reader.hasError() && fields != QSet<QString>{"ModelName", "CommunicationConfig", "Channels"})
        reader.raiseError("Incomplete Instrument configuration");
    return ic;
}

void InstrumentSetupModel::loadChannelsFromXml(QXmlStreamReader& reader, InstrumentConfig& ic, bool numbered)
{
    const bool requireNumber = numbered && reader.attributes().value("layout") != "legacy";
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Channels")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Channel") {
            ChannelSetting ch;
            if (requireNumber && !reader.attributes().hasAttribute("number")) {
                reader.raiseError("Missing physical channel number");
                return;
            }
            if (reader.attributes().hasAttribute("number")) {
                bool ok = false;
                int number = reader.attributes().value("number").toInt(&ok);
                if (!ok || number <= 0 || ic.channelNumbers.contains(number)) {
                    reader.raiseError("Invalid or duplicate physical channel number");
                    return;
                }
                ic.channelNumbers.append(number);
            }
            ch.subModel = reader.attributes().value("subModel").toString();
            ch.index = reader.attributes().value("index").toInt();
            const QString syncType = reader.attributes().value("syncType").toString().trimmed().toUpper();
            if (syncType == "NONE")
                ch.syncType = 0;
            else if (syncType == "MASTER")
                ch.syncType = 1;
            else if (syncType == "SLAVE")
                ch.syncType = 2;
            else if (!syncType.isEmpty()) {
                reader.raiseError("Invalid channel syncType: " + syncType);
                return;
            }
            ic.channels.append(ch);
            reader.skipCurrentElement();
        }
    }
    if (!ic.channelNumbers.isEmpty() && ic.channelNumbers.size() != ic.channels.size())
        reader.raiseError("Incomplete physical channel numbering");
}
