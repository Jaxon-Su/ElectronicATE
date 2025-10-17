#pragma once

#include <QObject>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QDomElement>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"

class Page1Model : public QObject {
    Q_OBJECT
public:
    explicit Page1Model(QObject *parent = nullptr);

    // Getters
    int loadOutputs() const { return m_config.loadOutputs; }
    int relayOutputs() const { return m_config.relayOutputs; }
    const Page1Config& getConfig() const { return m_config; }
    const QMap<QString, QStringList>& getXmlModelMap() const { return xmlModelMap; }
    const QMap<QString, QStringList>& getSubModelMap() const { return subModelMap; }
    const QMap<QString, QStringList>& getChannelsMap() const { return channelsMap; }

    // Setters
    void setConfig(const Page1Config& cfg) { m_config = cfg; }
    void setInstrumentConfigs(const QList<InstrumentConfig>& configs);
    void setLoadOutputs(int value) { m_config.loadOutputs = value; }
    void setRelayOutputs(int value) { m_config.relayOutputs = value; }

    // XML operations
    bool loadBaseXml(const QString& fileName);
    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

signals:
    void configLoaded(const Page1Config &cfg);

private:
    // Helper methods for loadBaseXml
    bool openAndParseXmlFile(const QString& fileName, QDomDocument& doc);
    void clearAllMaps();
    void collectTemplates(const QDomNodeList& nodes);
    void processInstruments(const QDomNodeList& nodes);

    InstrumentConfig createInstrumentConfig(const QDomElement& element);
    QList<QDomElement> getModelElements(const QDomElement& element);
    void processModels(const QList<QDomElement>& models, const QString& type, const QString& instrumentName);
    void processModel(const QDomElement& modelElement, const QString& type, const QString& modelName);

    QStringList extractSubModels(const QDomElement& modelElement);
    QStringList extractChannels(const QDomElement& modelElement);

    // Helper methods for loadXml
    void loadInstrumentsFromXml(QXmlStreamReader& reader, Page1Config& config);
    InstrumentConfig loadInstrumentFromXml(QXmlStreamReader& reader);
    void loadChannelsFromXml(QXmlStreamReader& reader, InstrumentConfig& ic);

    // Helper methods for writeXml
    void writeInstruments(QXmlStreamWriter& writer) const;
    void writeInstrument(QXmlStreamWriter& writer, const InstrumentConfig& ic) const;
    void writeChannels(QXmlStreamWriter& writer, const InstrumentConfig& ic) const;

private:
    Page1Config m_config;
    QStringList modelNames;
    QMap<QString, QStringList> xmlModelMap;
    QMap<QString, QStringList> subModelMap;
    QMap<QString, QStringList> channelsMap; //QMap(("6304", QList("1", "2", "3", "4"))("6314", QList("1", "3", "5", "7"))("6334A", QList("1", "3", "5", "7"))("DE-A3000AB", QList())("DPO7000", QList())("Relay_RTU", QList("1", "2", "3", "4")))
    QMap<QString, QDomElement> templateMap;
};
