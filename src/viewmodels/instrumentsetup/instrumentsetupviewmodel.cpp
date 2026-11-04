#include "instrumentsetupviewmodel.h"
#include "instrumentsetupmodel.h"
#include <QSet>
#include "channelnumberpolicy.h"
#include "chroma6310frame.h"
#include "chroma63600frame.h"
#include <QDebug>
#include <QPointer>

InstrumentSetupViewModel::InstrumentSetupViewModel(InstrumentSetupModel* model, QObject* parent) : QObject(parent), m_model(model)
{
    connect(m_model, &InstrumentSetupModel::configLoaded, this, &InstrumentSetupViewModel::onConfigLoaded);

    onConfigLoaded(m_model->getConfig());
}

// 查詢接口

QStringList InstrumentSetupViewModel::subModels(const QString& modelName, int channel, const QMap<int, QString>& installed) const
{
    QStringList choices;
    for (const auto &module : m_model->getSubModelMap().value(modelName)) {
        if (!Chroma63600Frame::allows(modelName, channel, module)) continue;
        if (Chroma63600Frame::lastChannel(modelName) && channel % 2 == 0 &&
            !installed.value(channel - 1).isEmpty() && installed.value(channel - 1) != module) continue;
        if (Chroma6310Frame::isFrame(modelName)) {
            if (!Chroma6310Frame::allows(modelName, channel, module)) continue;
            bool fits = true;
            for (auto it = installed.cbegin(); it != installed.cend(); ++it)
                if (it.key() != channel && !it.value().isEmpty() &&
                    !Chroma6310Frame::canShareFrame(channel, module, it.key(), it.value())) { fits = false; break; }
            if (!fits) continue;
        }
        choices << module;
    }
    return choices;
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
    m_channels = m_model->catalogChannels();
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
    if (inst.channels.isEmpty()) return;
    if (m_channels.isEmpty()) {
        if (inst.channelNumbers.isEmpty())
            inst.channelNumbers = ChannelNumberPolicy::assign(inst.channels.size(), m_channels);
        return;
    }
    if (inst.channelNumbers == m_channels) return;
    const auto numbers = inst.channelNumbers.isEmpty() ? m_channels : inst.channelNumbers;
    QList<ChannelSetting> aligned(m_channels.size(), ChannelSetting{});
    for (int i = 0; i < inst.channels.size(); ++i) {
        const int col = m_channels.indexOf(numbers.value(i, -1));
        if (col >= 0) aligned[col] = inst.channels[i];
    }
    inst.channels = aligned;
    inst.channelNumbers = m_channels;
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
    candidate.loadXml(reader, m_model);
}

std::function<void()> InstrumentSetupViewModel::captureXmlRollback()
{
    // A draft with conflicting mappings can be replaced by a valid file even
    // though saving that draft is forbidden. Restore the exact draft on failure.
    return [this, config = currentConfig()] {
        m_model->setConfig(config);
        onConfigLoaded(config);
    };
}

void InstrumentSetupViewModel::publishXmlLoaded()
{
    onConfigLoaded(m_model->getConfig());
}
