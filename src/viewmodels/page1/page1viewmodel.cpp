#include "page1viewmodel.h"
#include "page1model.h"
#include <QSet>
#include <algorithm>
#include <QDebug>
#include <QCoreApplication>

Page1ViewModel::Page1ViewModel(Page1Model *model, QObject *parent)
    : QObject(parent), m_model(model)
{
    connect(m_model, &Page1Model::configLoaded,
            this,    &Page1ViewModel::onConfigLoaded);

    // 使用應用程式目錄的相對路徑
    QString xmlPath = QCoreApplication::applicationDirPath() + "/XML/Instrument.xml";
    m_model->loadBaseXml(xmlPath);

    //以下硬編碼的絕對路徑build會有問題
    // m_model->loadBaseXml("C:/Qt_Project/ElectronicATE/XML/Instrument.xml");
}

// ==================== 查詢接口 ====================

QStringList Page1ViewModel::subModels(const QString &modelName) const
{
    return m_model->getSubModelMap().value(modelName);
}

QSet<int> Page1ViewModel::channelsOfModel(const QString &modelName) const
{
    QSet<int> channelSet;
    const auto &channelStrings = m_model->getChannelsMap().value(modelName);

    for (const QString &ch : channelStrings) {
        bool ok;
        int channelNum = ch.toInt(&ok);
        if (ok) channelSet.insert(channelNum);
    }

    return channelSet;
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
    notifyViewModelReady();
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

    // qDebug() << "m_model->getChannelsMap() = " << m_model->getChannelsMap();

    for (const auto &channelList : m_model->getChannelsMap()) {
        for (const QString &chStr : channelList) {
            bool ok;
            int channelNum = chStr.toInt(&ok); //轉換失敗ok = false , 成功 = true
            if (ok) channelSet.insert(channelNum);
        }
    }

    m_channels = channelSet.values();
    std::sort(m_channels.begin(), m_channels.end()); //ex:QList(1, 2, 3, 4, 5, 7) 紀錄所有儀器instruments channels不重複數值
}

void Page1ViewModel::notifyViewModelReady()
{
    emit dataChanged();
    emit loadOutputsChanged(m_model->loadOutputs());
    emit relayOutputsChanged(m_model->relayOutputs());
}

// ==================== UI 配置變更處理 ====================

void Page1ViewModel::onUiConfigChanged(const QList<InstrumentConfig>& configs,
                                       int loadOutputs, int relayOutputs)
{
    updateOutputSettings(loadOutputs, relayOutputs);
    updateInstrumentConfigs(configs);

    emit configUpdated(m_model->getConfig());
}

void Page1ViewModel::updateOutputSettings(int loadOutputs, int relayOutputs)
{
    setLoadOutputs(loadOutputs);
    setRelayOutputs(relayOutputs);
}

void Page1ViewModel::updateInstrumentConfigs(const QList<InstrumentConfig>& configs)
{
    QList<InstrumentConfig> enrichedConfigs = configs;
    enrichConfigsWithChannelNumbers(enrichedConfigs);

    if (m_model) {
        m_model->setInstrumentConfigs(enrichedConfigs);
    }
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
    inst.channelNumbers.clear();

    for (int i = 0; i < inst.channels.size(); ++i) {
        int channelNum = (i < m_channels.size()) ? m_channels[i] : -1;
        inst.channelNumbers << channelNum;
    }
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
