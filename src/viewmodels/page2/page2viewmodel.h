#pragma once

#include <QObject>
#include <QStringList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2model.h"
#include "page1config.h"
#include "ixmlserializable.h"
#include "itestconditionprovider.h"
#include "tableheaderbuilder.h"

// Page2 的 ViewModel - 負責業務邏輯和 UI-Model 數據轉換
class Page2ViewModel : public QObject, public IXmlSerializable, public ITestConditionProvider
{
    Q_OBJECT
public:
    explicit Page2ViewModel(Page2Model* model, QObject *parent = nullptr);
    TestConditionSnapshot conditions() const { return m_model->snapshot(); }

    // 獲取 Load 的名稱列表
    QStringList loadNameList() const;

    // 獲取指定表格的標題列表（供 Page3 ComboBox 使用）
    QStringList TitleList(TableKind type) const;

    // IXmlSerializable
    QString xmlTagName() const override { return "Page2"; }
    void writeXml(QXmlStreamWriter& writer) const override;
    void validateXml(QXmlStreamReader& reader) const override;
    void loadXml(QXmlStreamReader& reader) override;

    // Model 數據代理訪問
    const QVector<InputRow>&      inputRows()    const override { return m_model->getInputRows(); }
    const QVector<DcRow>&         dcRows()       const { return m_model->getDcRows(); }
    const QVector<RelayDataRow>&  relayRows()    const override { return m_model->getRelayRows(); }
    const LoadMetaRow&            loadMeta()     const override { return m_model->getLoadMeta(); }
    const QVector<LoadDataRow>&   loadRows()     const override { return m_model->getLoadRows(); }
    const DynamicMetaRow&         dynamicMeta()  const override { return m_model->getDynamicMeta(); }
    const QVector<DynamicDataRow>& dynamicRows() const override { return m_model->getDynamicRows(); }

    int maxOutput() const;           // Load/Dynamic 的最大輸出數
    int maxRelayOutput() const { return m_maxRelayOutput; }

    // Model 數據設置
    void setInputRows(const QVector<InputRow>& rows);
    void setDcRows(const QVector<DcRow>& rows);
    void setRelayRows(const QVector<RelayDataRow>& rows);
    void setLoadMeta(const LoadMetaRow& meta);
    void setLoadRows(const QVector<LoadDataRow>& rows);
    void setDynamicMeta(const DynamicMetaRow& meta);
    void setDynamicRows(const QVector<DynamicDataRow>& rows);

    // Power 計算（Load 表格專用）
    double calcRowPower(int dataRow) const;
    double calcDynamicRowPower(int dataRow) const;
    void broadcastAllPowers();
    void broadcastAllDynamicPowers();
    QStringList loadRangeOptions(int outputIndex, const QString& baseMode) const;
    QStringList dynamicRangeOptions(int outputIndex) const;

public slots:
    void setConditions(const TestConditionSnapshot& snapshot);
    void setMaxOutput(int maxOutput);
    void setMaxRelayOutput(int maxRelayOutput);

    void addRow(TableKind kind);
    void removeRow(TableKind kind);
    void cellValueChanged(TableKind kind, int row, int col, const QString &text);

    void refreshUIOutputs();
    void onConfigLoaded();
    void onPage1ConfigChanged(const Page1Config& cfg);

    // Page2 同步回調
    void onInputRowsChanged(const QVector<InputRow>& rows)         { setInputRows(rows); }
    void onDcRowsChanged(const QVector<DcRow>& rows)               { setDcRows(rows); }
    void onRelayRowsChanged(const QVector<RelayDataRow>& rows)     { setRelayRows(rows); }
    void onLoadMetaChanged(const LoadMetaRow& meta)                { setLoadMeta(meta); }
    void onLoadRowsChanged(const QVector<LoadDataRow>& rows)       { setLoadRows(rows); }
    void onDynamicMetaChanged(const DynamicMetaRow& meta)          { setDynamicMeta(meta); }
    void onDynamicRowsChanged(const QVector<DynamicDataRow>& rows) { setDynamicRows(rows); }

signals:
    void conditionsChanged(const TestConditionSnapshot& snapshot);
    // UI 更新信號
    void headersChanged(TableKind kind, const QStringList &headers);
    void relayRowsStructChanged(const QVector<RelayDataRow>&);
    void rowAddRequested(TableKind kind, const QStringList &validatorTags);
    void rowRemoveRequested(TableKind kind);
    void inputTitleChanged(int row, const QString &display);
    void powerUpdated(int row, double value);
    void dynamicPowerUpdated(int row, double value);

    // 標題列表變更（通知 Page3）
    void titleListChanged(TableKind type, const QStringList& titles);

    // 數據變更
    void dataChanged();
    void inputRowsStructChanged(const QVector<InputRow>&);
    void dcRowsStructChanged(const QVector<DcRow>&);
    void loadMetaStructChanged(const LoadMetaRow&);
    void loadRowsStructChanged(const QVector<LoadDataRow>&);
    void dynamicMetaStructChanged(const DynamicMetaRow&);
    void dynamicRowsStructChanged(const QVector<DynamicDataRow>&);

private:
    bool publishConditions();
    quint64 m_conditionRevision = 0;
    // Meta 行數常量
    static constexpr int META_ROWS    = 5;  // Load 表格
    static constexpr int META_ROWS_Dy = 3;  // Dynamic 表格
    static constexpr int META_ROWS_Relay = 0;
    static constexpr int META_ROWS_Dc    = 0;

    int m_maxOutput = 0;
    int m_maxRelayOutput = 0;
    Page2Model* m_model = nullptr;
    Page1Config m_page1Config;
};
