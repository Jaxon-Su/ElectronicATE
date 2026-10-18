#pragma once

#include <QObject>
#include <QStringList>
#include <QList>
#include "instrumentsetupmodel.h"
#include "instrumentsetupconfig.h"
#include "ixmlserializable.h"

class InstrumentSetupModel;
struct InstrumentSetupConfig;

class InstrumentSetupViewModel : public QObject, public IXmlSerializable {
    Q_OBJECT
  public:
    explicit InstrumentSetupViewModel(InstrumentSetupModel* model, QObject* parent = nullptr);

    QList<TableRowInfo> rows() const { return m_rows; }
    QList<int> channels() const { return m_channels; }
    const InstrumentSetupConfig& currentConfig() const { return m_model->getConfig(); }
    int loadOutputs() const { return m_model ? m_model->loadOutputs() : 1; }
    int dcInputs() const { return m_model ? m_model->getConfig().dcInputs : 1; }
    int relayOutputs() const { return m_model ? m_model->relayOutputs() : 1; }

    QStringList subModels(const QString& modelName) const;
    QSet<int> channelsOfModel(const QString& modelName) const;
    bool hasChannelInterface(const QString& instName) const;

    QString xmlTagName() const override { return "InstrumentSetup"; }
    void writeXml(QXmlStreamWriter& writer) const override;
    void publishXmlLoaded() override;
    void validateXml(QXmlStreamReader& reader) const override;
    void loadXml(QXmlStreamReader& reader) override;

  public slots:
    void setLoadOutputs(int value);
    void setRelayOutputs(int value);
    void onUiConfigChanged(const QList<InstrumentConfig>& configs, int loadOutputs, int relayOutputs,
                           int dcInputs = -1);

  signals:
    void dataChanged();
    void loadOutputsChanged(int value);
    void relayOutputsChanged(int value);
    void configUpdated(const InstrumentSetupConfig& cfg);

  private slots:
    void onConfigLoaded(const InstrumentSetupConfig& cfg);

  private:
    // 初始化輔助函式
    void buildTableRows(const InstrumentSetupConfig& cfg);
    void buildChannelList();

    // 配置更新輔助函式

    void enrichConfigsWithChannelNumbers(QList<InstrumentConfig>& configs) const;
    void assignChannelNumbers(InstrumentConfig& inst) const;

    // 工具函式
    bool isChannelBasedType(const QString& type) const;

    quint64 m_uiConfigRevision = 0;
    InstrumentSetupModel* m_model = nullptr;
    QList<TableRowInfo> m_rows;
    QList<int> m_channels;
};
