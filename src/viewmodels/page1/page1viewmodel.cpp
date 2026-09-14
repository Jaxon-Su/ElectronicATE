#include "page1viewmodel.h"
#include "page1model.h"
#include <QSet>
#include "channelnumberpolicy.h"
#include <QDebug>
#include <QPointer>


Page1ViewModel::Page1ViewModel(Page1Model *model, QObject *parent)
    : QObject(parent), m_model(model)
{
    connect(m_model, &Page1Model::configLoaded,
            this,    &Page1ViewModel::onConfigLoaded);

    onConfigLoaded(m_model->getConfig());
}

// ==================== 查詢接口 ====================

QStringList Page1ViewModel::subModels(const QString &modelName) const
{
    return m_model->getSubModelMap().value(modelName);
}

QSet<int> Page1ViewModel::channelsOfModel(const QString &modelName) const
{
    return ChannelNumberPolicy::parse(m_model->getChannelsMap().value(modelName));
}

bool Page1ViewModel::hasChannelInterface(const QString &instName) const
{
    for (const auto &row : m_rows) {
        if (row.instrument == instName)
            return row.hasChannels;
    }
    return false;
}

// ==================== Outputs 設置 ====================

void Page1ViewModel::setLoadOutputs(int value)
{
    if (!m_model || value == m_model->loadOutputs())
        return;

    m_model->setLoadOutputs(value);
    emit loadOutputsChanged(value);
}

void Page1ViewModel::setRelayOutputs(int value)
{
    if (!m_model || value == m_model->relayOutputs())
        return;

    m_model->setRelayOutputs(value);
    emit relayOutputsChanged(value);
}

// ==================== 配置載入處理 ====================

void Page1ViewModel::onConfigLoaded(const Page1Config &cfg)
{
    buildTableRows(cfg);
    buildChannelList();

    QList<InstrumentConfig> enrichedConfigs = cfg.instruments;
    enrichConfigsWithChannelNumbers(enrichedConfigs);
    Page1Config enrichedCfg = cfg;
    enrichedCfg.instruments = enrichedConfigs;
    if (m_model)
        m_model->setInstrumentConfigs(enrichedConfigs);

    const auto revision = ++m_uiConfigRevision;
    QPointer<Page1ViewModel> alive(this);
    emit dataChanged();
    if (!alive || revision != m_uiConfigRevision) return;
    emit loadOutputsChanged(enrichedCfg.loadOutputs);
    if (!alive || revision != m_uiConfigRevision) return;
    emit relayOutputsChanged(enrichedCfg.relayOutputs);
    if (!alive || revision != m_uiConfigRevision) return;
    emit configUpdated(enrichedCfg);
}

void Page1ViewModel::buildTableRows(const Page1Config &cfg)
{
    m_rows.clear();
    const auto &xmlMap = m_model->getXmlModelMap();

    for (const auto &inst : cfg.instruments) {
        bool hasChannels = isChannelBasedType(inst.type);
        TableRowInfo row(inst.name, inst.type, hasChannels, xmlMap.value(inst.name));
        m_rows << row;
    }
}

void Page1ViewModel::buildChannelList()
{
    QSet<int> channelSet;
    for (const auto& channelList : m_model->getChannelsMap())
        channelSet.unite(ChannelNumberPolicy::parse(channelList));
    m_channels = ChannelNumberPolicy::sorted(channelSet);
}

// ==================== UI 配置變更處理 ====================

void Page1ViewModel::onUiConfigChanged(const QList<InstrumentConfig>& configs,
                                       int loadOutputs, int relayOutputs)
{
    const bool loadChanged = m_model->loadOutputs() != loadOutputs;
    const bool relayChanged = m_model->relayOutputs() != relayOutputs;
    Page1Config updated = m_model->getConfig();
    updated.loadOutputs = loadOutputs;
    updated.relayOutputs = relayOutputs;
    updated.instruments = configs;
    enrichConfigsWithChannelNumbers(updated.instruments);
    m_model->setConfig(updated);

    const auto revision = ++m_uiConfigRevision;
    QPointer<Page1ViewModel> alive(this);
    if (loadChanged) emit loadOutputsChanged(loadOutputs);
    if (!alive || revision != m_uiConfigRevision) return;
    if (relayChanged) emit relayOutputsChanged(relayOutputs);
    if (!alive || revision != m_uiConfigRevision) return;
    emit configUpdated(updated);
}
void Page1ViewModel::enrichConfigsWithChannelNumbers(QList<InstrumentConfig>& configs) const
{
    for (auto& inst : configs) {
        if (isChannelBasedType(inst.type)) {
            assignChannelNumbers(inst);
        }
    }
}

void Page1ViewModel::assignChannelNumbers(InstrumentConfig& inst) const
{
    inst.channelNumbers = ChannelNumberPolicy::assign(inst.channels.size(), m_channels);
}

// ==================== XML 序列化 ====================

void Page1ViewModel::writeXml(QXmlStreamWriter& writer) const
{
    if (m_model) m_model->writeXml(writer);
}

void Page1ViewModel::loadXml(QXmlStreamReader& reader)
{
    if (m_model) m_model->loadXml(reader);
}

// ==================== 工具函式 ====================

bool Page1ViewModel::isChannelBasedType(const QString &type) const
{
    return (type == "Load" || type == "Relay");
}

void Page1ViewModel::validateXml(QXmlStreamReader& reader) const
{
    Page1Model candidate;
    candidate.loadXml(reader);
}
