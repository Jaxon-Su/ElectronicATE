#include "instrumentsetupmodel.h"
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include <QSet>

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
    writer.writeStartElement("InstrumentSetup");
    writer.writeAttribute("schemaVersion", "2");
    writer.writeTextElement("LoadOutputs", QString::number(m_config.loadOutputs));
    writer.writeTextElement("RelayOutputs", QString::number(m_config.relayOutputs));
    writer.writeTextElement("DcInputs", QString::number(m_config.dcInputs));
    writeInstruments(writer);
    writer.writeEndElement(); // InstrumentSetup
}

void InstrumentSetupModel::loadXml(QXmlStreamReader& reader)
{
    if (!reader.isStartElement() || reader.name() != QStringLiteral("InstrumentSetup")) {
        reader.raiseError(QStringLiteral("Expected InstrumentSetup element"));
        return;
    }
    if (reader.attributes().value("schemaVersion") != "2") {
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
            loadInstrumentsFromXml(reader, config);
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
    for (const auto& ch : ic.channels) {
        writer.writeStartElement("Channel");
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

void InstrumentSetupModel::loadInstrumentsFromXml(QXmlStreamReader& reader, InstrumentSetupConfig& config)
{
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Instruments")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Instrument") {
            InstrumentConfig ic = loadInstrumentFromXml(reader);
            config.instruments.append(ic);
        }
    }
}

InstrumentConfig InstrumentSetupModel::loadInstrumentFromXml(QXmlStreamReader& reader)
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
            loadChannelsFromXml(reader, ic);
        } else {
            reader.raiseError("Unexpected Instrument field: " + elementName);
            return ic;
        }
    }

    if (!reader.hasError() && fields != QSet<QString>{"ModelName", "CommunicationConfig", "Channels"})
        reader.raiseError("Incomplete Instrument configuration");
    return ic;
}

void InstrumentSetupModel::loadChannelsFromXml(QXmlStreamReader& reader, InstrumentConfig& ic)
{
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Channels")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Channel") {
            ChannelSetting ch;
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
}
