#pragma once

#include <QObject>
#include <QStringList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2model.h"

// Page2 的 ViewModel - 負責業務邏輯和 UI-Model 數據轉換
class Page2ViewModel : public QObject
{
    Q_OBJECT
public:
    explicit Page2ViewModel(Page2Model* model, QObject *parent = nullptr);

    // 獲取 Load 的名稱列表
    QStringList loadNameList() const;

    // 獲取指定表格的標題列表（供 Page3 ComboBox 使用）
    QStringList TitleList(LoadKind type) const;

    // XML 序列化
    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    // Model 數據代理訪問
    const QVector<InputRow>& inputRows() const      { return m_model->inputRows; }
    const QVector<RelayDataRow>& relayRows() const  { return m_model->relayRows; }
    const LoadMetaRow& loadMeta() const            { return m_model->loadMeta; }
    const QVector<LoadDataRow>& loadRows() const   { return m_model->loadRows; }
    const DynamicMetaRow& dynamicMeta() const      { return m_model->dynamicMeta; }
    const QVector<DynamicDataRow>& dynamicRows() const { return m_model->dynamicRows; }

    int maxOutput() const;           // Load/Dynamic 的最大輸出數
    int maxRelayOutput() const { return m_maxRelayOutput; }

    // Model 數據設置
    void setInputRows(const QVector<InputRow>& rows);
    void setRelayRows(const QVector<RelayDataRow>& rows);
    void setLoadMeta(const LoadMetaRow& meta);
    void setLoadRows(const QVector<LoadDataRow>& rows);
    void setDynamicMeta(const DynamicMetaRow& meta);
    void setDynamicRows(const QVector<DynamicDataRow>& rows);

    // Power 計算（Load 表格專用）
    double calcRowPower(int dataRow) const;     // dataRow 是數據行索引（不含 Meta 行）
    void broadcastAllPowers();                  // 廣播所有行的 Power 更新

public slots:
    // 設置輸出數量（會調整 Meta 長度並更新表頭）
    void setMaxOutput(int maxOutput);
    void setMaxRelayOutput(int maxRelayOutput);

    // 行操作
    void addRow(LoadKind kind);
    void removeRow(LoadKind kind);

    // 單元格值變更處理
    void cellValueChanged(LoadKind kind, int row, int col, const QString &text);

    // UI 刷新
    void refreshUIOutputs();
    void onConfigLoaded();

    // Page2 同步回調
    void onInputRowsChanged(const QVector<InputRow>& rows)        { setInputRows(rows); }
    void onRelayRowsChanged(const QVector<RelayDataRow>& rows)    { setRelayRows(rows); }
    void onLoadMetaChanged(const LoadMetaRow& meta)               { setLoadMeta(meta); }
    void onLoadRowsChanged(const QVector<LoadDataRow>& rows)      { setLoadRows(rows); }
    void onDynamicMetaChanged(const DynamicMetaRow& meta)         { setDynamicMeta(meta); }
    void onDynamicRowsChanged(const QVector<DynamicDataRow>& rows){ setDynamicRows(rows); }

signals:
    // UI 更新信號
    void headersChanged(LoadKind kind, const QStringList &headers);
    void relayRowsStructChanged(const QVector<RelayDataRow>&);
    void rowAddRequested(LoadKind kind, const QStringList &validatorTags);
    void rowRemoveRequested(LoadKind kind);
    void inputTitleChanged(int row, const QString &display);
    void powerUpdated(int row, double value);

    // 標題列表變更（通知 Page3）
    void TitleListChanged(LoadKind type, const QStringList& titles);

    // 數據變更
    void dataChanged();
    void inputRowsStructChanged(const QVector<InputRow>&);
    void loadMetaStructChanged(const LoadMetaRow&);
    void loadRowsStructChanged(const QVector<LoadDataRow>&);
    void dynamicMetaStructChanged(const DynamicMetaRow&);
    void dynamicRowsStructChanged(const QVector<DynamicDataRow>&);

private:
    // Meta 行數常量
    static constexpr int META_ROWS = 8;        // Load 表格
    static constexpr int META_ROWS_Dy = 6;     // Dynamic 表格
    static constexpr int META_ROWS_Relay = 0;  // Relay 表格

    int m_maxRelayOutput = 1;
    Page2Model* m_model = nullptr;
};
