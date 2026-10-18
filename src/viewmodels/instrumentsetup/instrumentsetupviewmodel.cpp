#include "instrumentsetupviewmodel.h"
#include "instrumentsetupmodel.h"
#include <QSet>
#include "channelnumberpolicy.h"
#include <QDebug>
#include <QPointer>

InstrumentSetupViewModel::InstrumentSetupViewModel(InstrumentSetupModel* model, QObject* parent) : QObject(parent), m_model(model)
{
    connect(m_model, &InstrumentSetupModel::configLoaded, this, &InstrumentSetupViewModel::onConfigLoaded);

    onConfigLoaded(m_model->getConfig());
}

// 查詢接口

QStringList InstrumentSetupViewModel::subModels(const QString& modelName) const
{
    return m_model->getSubModelMap().value(modelName);
}

QSet<int> InstrumentSetupViewModel::channelsOfModel(const QString& modelName) const
{
    return ChannelNumberPolicy::parse(m_model->getChannelsMap().value(modelName));
}

bool InstrumentSetupViewModel::hasChannelInterface(const QString& instName) const
{
    for (const auto& row : m_rows) {
        if (row.instrument == instName)
            return row.hasChannels;
    }
    return false;
}

// Outputs 設置

void InstrumentSetupViewModel::setLoadOutputs(int value)
{
    if (!m_model || value == m_model->loadOutputs())
        return;

    m_model->setLoadOutputs(value);
    emit loadOutputsChanged(value);
}

void InstrumentSetupViewModel::setRelayOutputs(int value)
{
    if (!m_model || value == m_model->relayOutputs())
        return;

    m_model->setRelayOutputs(value);
    emit relayOutputsChanged(value);
}

// 配置載入處理

void InstrumentSetupViewModel::onConfigLoaded(const InstrumentSetupConfig& cfg)
{
    buildTableRows(cfg);
    buildChannelList();

    QList<InstrumentConfig> enrichedConfigs = cfg.instruments;
    enrichConfigsWithChannelNumbers(enrichedConfigs);
    InstrumentSetupConfig enrichedCfg = cfg;
    enrichedCfg.instruments = enrichedConfigs;
    if (m_model)
        m_model->setInstrumentConfigs(enrichedConfigs);

    const auto revision = ++m_uiConfigRevision;
    QPointer<InstrumentSetupViewModel> alive(this);
    emit dataChanged();
    if (!alive || revision != m_uiConfigRevision)
        return;
    emit loadOutputsChanged(enrichedCfg.loadOutputs);
    if (!alive || revision != m_uiConfigRevision)
        return;
    emit relayOutputsChanged(enrichedCfg.relayOutputs);
    if (!alive || revision != m_uiConfigRevision)
        return;
    emit configUpdated(enrichedCfg);
}

void InstrumentSetupViewModel::buildTableRows(const InstrumentSetupConfig& cfg)
{
    m_rows.clear();
    const auto& xmlMap = m_model->getXmlModelMap();

    for (const auto& inst : cfg.instruments) {
        bool hasChannels = isChannelBasedType(inst.type);
        TableRowInfo row(inst.name, inst.type, hasChannels, xmlMap.value(inst.name));
        m_rows << row;
    }
}

void InstrumentSetupViewModel::buildChannelList()
{
    QSet<int> channelSet;
    for (const auto& channelList : m_model->getChannelsMap())
        channelSet.unite(ChannelNumberPolicy::parse(channelList));
    m_channels = ChannelNumberPolicy::sorted(channelSet);
}

// UI 配置變更處理

void InstrumentSetupViewModel::onUiConfigChanged(const QList<InstrumentConfig>& configs, int loadOutputs,
                                       int relayOutputs, int dcInputs)
{
    const bool loadChanged = m_model->loadOutputs() != loadOutputs;
    const bool relayChanged = m_model->relayOutputs() != relayOutputs;
    InstrumentSetupConfig updated = m_model->getConfig();
    updated.loadOutputs = loadOutputs;
    updated.relayOutputs = relayOutputs;
    if (dcInputs != -1)
        updated.dcInputs = qBound(1, dcInputs, 3);
    updated.instruments = configs;
    enrichConfigsWithChannelNumbers(updated.instruments);
    m_model->setConfig(updated);

    const auto revision = ++m_uiConfigRevision;
    QPointer<InstrumentSetupViewModel> alive(this);
    if (loadChanged)
        emit loadOutputsChanged(loadOutputs);
    if (!alive || revision != m_uiConfigRevision)
        return;
    if (relayChanged)
        emit relayOutputsChanged(relayOutputs);
    if (!alive || revision != m_uiConfigRevision)
        return;
    emit configUpdated(updated);
}
void InstrumentSetupViewModel::enrichConfigsWithChannelNumbers(QList<InstrumentConfig>& configs) const
{
    for (auto& inst : configs) {
        if (isChannelBasedType(inst.type)) {
            assignChannelNumbers(inst);
        }
    }
}

void InstrumentSetupViewModel::assignChannelNumbers(InstrumentConfig& inst) const
{
    inst.channelNumbers = ChannelNumberPolicy::assign(inst.channels.size(), m_channels);
}

// XML 序列化

void InstrumentSetupViewModel::writeXml(QXmlStreamWriter& writer) const
{
    if (m_model)
        m_model->writeXml(writer);
}

void InstrumentSetupViewModel::loadXml(QXmlStreamReader& reader)
{
    if (m_model)
        m_model->loadXml(reader);
}

// 工具函式

bool InstrumentSetupViewModel::isChannelBasedType(const QString& type) const
{
    return (type == "Load" || type == "Relay");
}

void InstrumentSetupViewModel::validateXml(QXmlStreamReader& reader) const
{
    InstrumentSetupModel candidate;
    candidate.loadXml(reader);
}

void InstrumentSetupViewModel::publishXmlLoaded()
{
    onConfigLoaded(m_model->getConfig());
}
