#pragma once

#include <QObject>
#include <QStringList>
#include <QList>
#include "page1model.h"
#include "page1config.h"

class Page1Model;
struct Page1Config;

class Page1ViewModel : public QObject {
    Q_OBJECT
public:
    explicit Page1ViewModel(Page1Model *model, QObject *parent = nullptr);

    QList<TableRowInfo> rows()      const { return m_rows; }
    QList<int>          channels()  const { return m_channels; }
    const Page1Config&  currentConfig() const { return m_model->getConfig(); }
    int                 loadOutputs() const { return m_model ? m_model->loadOutputs() : 1; }
    int                 relayOutputs() const { return m_model ? m_model->relayOutputs() : 1; }

    QStringList         subModels(const QString &modelName) const;
    QSet<int>           channelsOfModel(const QString &modelName) const;
    bool                hasChannelInterface(const QString &instName) const;

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

public slots:
    void setLoadOutputs(int value);
    void setRelayOutputs(int value);
    void onUiConfigChanged(const QList<InstrumentConfig>& configs, int loadOutputs, int relayOutputs);

signals:
    void dataChanged();
    void loadOutputsChanged(int value);
    void relayOutputsChanged(int value);
    void configUpdated(const Page1Config &cfg);

private slots:
    void onConfigLoaded(const Page1Config &cfg);

private:
    // ==================== 初始化輔助函式 ====================
    void buildTableRows(const Page1Config &cfg);
    void buildChannelList();
    void notifyViewModelReady();

    // ==================== 配置更新輔助函式 ====================
    void updateOutputSettings(int loadOutputs, int relayOutputs);
    void updateInstrumentConfigs(const QList<InstrumentConfig>& configs);
    void enrichConfigsWithChannelNumbers(QList<InstrumentConfig>& configs) const;
    void assignChannelNumbers(InstrumentConfig& inst) const;

    // ==================== 工具函式 ====================
    bool isChannelBasedType(const QString &type) const;

    Page1Model         *m_model = nullptr;
    QList<TableRowInfo> m_rows;
    QList<int>          m_channels;
};
