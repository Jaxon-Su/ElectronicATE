#include "page1model.h"
#include <QFile>
#include <QDomDocument>
#include <QDebug>

Page1Model::Page1Model(QObject *parent)
    : QObject(parent) {}

// ========== Public Methods ==========

void Page1Model::setInstrumentConfigs(const QList<InstrumentConfig>& configs) {
    m_config.instruments = configs;
}

bool Page1Model::loadBaseXml(const QString &fileName) {
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
    emit configLoaded(m_config);

    return true;
}

void Page1Model::writeXml(QXmlStreamWriter& writer) const {
    writer.writeStartElement("Page1");
    writer.writeTextElement("LoadOutputs", QString::number(m_config.loadOutputs));
    writer.writeTextElement("RelayOutputs", QString::number(m_config.relayOutputs));
    writeInstruments(writer);
    writer.writeEndElement(); // Page1
}

void Page1Model::loadXml(QXmlStreamReader& reader) {
    if (!reader.isStartElement() || reader.name() != QStringLiteral("Page1")) {
        reader.raiseError(QStringLiteral("Expected Page1 element"));
        return;
    }
    Page1Config config;

    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Page1")) {
        reader.readNext();

        if (!reader.isStartElement()) {
            continue;
        }

        if (reader.name() == "LoadOutputs") {
            config.loadOutputs = reader.readElementText().toInt();
        }
        else if (reader.name() == "RelayOutputs") {
            config.relayOutputs = reader.readElementText().toInt();
        }
        else if (reader.name() == "Instruments") {
            loadInstrumentsFromXml(reader, config);
        }
    }

    if (reader.hasError()) return;

    m_config = config;
    emit configLoaded(m_config);
}

// ========== Private Helper Methods for loadBaseXml ==========

bool Page1Model::openAndParseXmlFile(const QString& fileName, QDomDocument& doc) {
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

void Page1Model::clearAllMaps() {
    m_config.instruments.clear();
    xmlModelMap.clear();
    modelNames.clear();
    subModelMap.clear();
    channelsMap.clear();
    templateMap.clear();
}

void Page1Model::collectTemplates(const QDomNodeList& nodes) {
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement element = nodes.at(i).toElement();
        if (element.attribute("type") == "Template") {
            templateMap[element.attribute("name")] = element;
        }
    }
}

void Page1Model::processInstruments(const QDomNodeList& nodes) {
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

InstrumentConfig Page1Model::createInstrumentConfig(const QDomElement& element) {
    InstrumentConfig ic;
    ic.name = element.attribute("name");
    ic.type = element.attribute("type");
    ic.modelName = "";
    ic.address = "";
    ic.enabled = true;
    ic.channels.clear();
    return ic;
}

QList<QDomElement> Page1Model::getModelElements(const QDomElement& element) {
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

void Page1Model::processModels(const QList<QDomElement>& models, const QString& type, const QString& instrumentName) {
    for (const auto& modelElement : models) {
        QString modelName = modelElement.firstChildElement("modelName").text();
        processModel(modelElement, type, modelName);
    }
}

void Page1Model::processModel(const QDomElement& modelElement, const QString& type, const QString& modelName) {
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

QStringList Page1Model::extractSubModels(const QDomElement& modelElement) {
    QStringList subModels;
    QDomNodeList subModelNodes = modelElement.elementsByTagName("subModel");

    for (int i = 0; i < subModelNodes.count(); ++i) {
        subModels << subModelNodes.at(i).toElement().attribute("name");
    }

    return subModels;
}

QStringList Page1Model::extractChannels(const QDomElement& modelElement) {
    QString channelsText = modelElement.firstChildElement("channels").text();
    return channelsText.split(",", Qt::SkipEmptyParts);
}

// ========== Private Helper Methods for writeXml ==========

void Page1Model::writeInstruments(QXmlStreamWriter& writer) const {
    writer.writeStartElement("Instruments");
    for (const auto& ic : m_config.instruments) {
        writeInstrument(writer, ic);
    }
    writer.writeEndElement(); // Instruments
}

void Page1Model::writeInstrument(QXmlStreamWriter& writer, const InstrumentConfig& ic) const
{
    writer.writeStartElement("Instrument");
    writer.writeAttribute("name", ic.name);
    writer.writeAttribute("type", ic.type);
    writer.writeAttribute("enabled", ic.enabled ? "true" : "false");
    writer.writeTextElement("ModelName", ic.modelName);

    // 寫入地址（向後相容）
    writer.writeTextElement("Address", ic.address);

    // 新增：寫入完整的通訊配置
    if (ic.commConfig.isValid()) {
        ic.commConfig.writeXml(writer);
    }

    writeChannels(writer, ic);
    writer.writeEndElement(); // Instrument
}

void Page1Model::writeChannels(QXmlStreamWriter& writer, const InstrumentConfig& ic) const {
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

// ========== Private Helper Methods for loadXml ==========

void Page1Model::loadInstrumentsFromXml(QXmlStreamReader& reader, Page1Config& config) {
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Instruments")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Instrument") {
            InstrumentConfig ic = loadInstrumentFromXml(reader);
            config.instruments.append(ic);
        }
    }
}

InstrumentConfig Page1Model::loadInstrumentFromXml(QXmlStreamReader& reader)
{
    InstrumentConfig ic;
    ic.name = reader.attributes().value("name").toString();
    ic.type = reader.attributes().value("type").toString();
    ic.enabled = (reader.attributes().value("enabled") == "true");

    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == QString("Instrument"))) {
        reader.readNext();

        if (!reader.isStartElement()) {
            continue;
        }

        QString elementName = reader.name().toString();

        if (elementName == "ModelName") {
            ic.modelName = reader.readElementText();
        }
        else if (elementName == "Address") {
            ic.address = reader.readElementText();
        }
        else if (elementName == "CommunicationConfig") {
            // 新增：讀取完整的通訊配置
            ic.commConfig.readXml(reader);
        }
        else if (elementName == "Channels") {
            loadChannelsFromXml(reader, ic);
        }
    }

    // 如果沒有 CommunicationConfig，嘗試從 address 字串解析
    if (!ic.commConfig.isValid() && !ic.address.isEmpty()) {
        ic.commConfig = CommunicationConfig::fromResourceString(ic.address);
    }

    // 確保 address 與 commConfig 同步
    if (ic.commConfig.isValid() && ic.address.isEmpty()) {
        ic.address = ic.commConfig.toResourceString();
    }

    return ic;
}

void Page1Model::loadChannelsFromXml(QXmlStreamReader& reader, InstrumentConfig& ic) {
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "Channels")) {
        reader.readNext();

        if (reader.isStartElement() && reader.name() == "Channel") {
            ChannelSetting ch;
            ch.subModel = reader.attributes().value("subModel").toString();
            ch.index = reader.attributes().value("index").toInt();
            const QString syncType = reader.attributes().value("syncType").toString().trimmed().toUpper();
            if (syncType == "NONE" || syncType == "0")
                ch.syncType = 0;
            else if (syncType == "MASTER" || syncType == "MA" || syncType == "1")
                ch.syncType = 1;
            else if (syncType == "SLAVE" || syncType == "SL" || syncType == "2")
                ch.syncType = 2;
            ic.channels.append(ch);
            reader.skipCurrentElement();
        }
    }
}
