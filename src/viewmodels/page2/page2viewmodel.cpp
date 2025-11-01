#include "page2viewmodel.h"
#include <QRegularExpression>
#include <QEvent>
#include <QLineEdit>
#include <QTabWidget>
#include <QVector>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2model.h"

Page2ViewModel::Page2ViewModel(Page2Model* model, QObject *parent)
    : QObject(parent), m_model(model)
{
    connect(m_model, &Page2Model::configLoaded, this, &Page2ViewModel::onConfigLoaded);
}

// 設置 Load/Dynamic 表格的輸出數量
void Page2ViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0) return;

    // 強制調整 Load Meta 資料長度一致
    auto& meta = m_model->loadMeta;
    auto resizeVec = [maxOutput](QVector<QString>& v) {
        while (v.size() < maxOutput) v.append(QString());
        if (v.size() > maxOutput) v.resize(maxOutput);
    };
    resizeVec(meta.names);
    resizeVec(meta.vo);
    resizeVec(meta.modes);
    resizeVec(meta.von);
    resizeVec(meta.riseSlopeCCH);
    resizeVec(meta.fallSlopeCCH);
    resizeVec(meta.riseSlopeCCL);
    resizeVec(meta.fallSlopeCCL);

    // 同步調整 Dynamic Meta 資料長度（包含 vo 欄位）
    auto& dmeta = m_model->dynamicMeta;
    resizeVec(dmeta.vo);
    resizeVec(dmeta.von);
    resizeVec(dmeta.riseSlopeCCDH);
    resizeVec(dmeta.fallSlopeCCDH);
    resizeVec(dmeta.riseSlopeCCDL);
    resizeVec(dmeta.fallSlopeCCDL);

    // 發出表頭變更信號
    QStringList headers{ "Output" };
    for (int i = 1; i <= maxOutput; ++i)
        headers << QString("Index%1").arg(i);

    QStringList loadHeaders = headers;
    loadHeaders << "Power";
    QStringList dynHeaders = headers;
    dynHeaders << "T1~T2 (s)";

    emit headersChanged(LoadKind::Load, loadHeaders);
    emit headersChanged(LoadKind::DyLoad, dynHeaders);

    broadcastAllPowers();
}

// 設置 Relay 表格的輸出數量
void Page2ViewModel::setMaxRelayOutput(int maxRelayOutput)
{
    if (maxRelayOutput <= 0) return;

    m_maxRelayOutput = maxRelayOutput;

    QStringList relayHeaders{ "Relay" };
    for (int i = 1; i <= maxRelayOutput; ++i)
        relayHeaders << QString("Index%1").arg(i);

    emit headersChanged(LoadKind::Relay, relayHeaders);
}

// 添加行（根據表格類型生成對應的驗證器標籤）
void Page2ViewModel::addRow(LoadKind kind)
{
    QStringList tags;

    switch (kind) {
    case LoadKind::Input:
        tags << "double" << "double" << "double";
        break;
    case LoadKind::Relay: {
        int relayOutputs = maxRelayOutput();
        for (int i = 0; i < relayOutputs; ++i)
            tags << "combo";
        break;
    }
    case LoadKind::Load: {
        int loadOutputs = maxOutput();
        for (int i = 0; i < loadOutputs; ++i)
            tags << "d";
        break;
    }
    case LoadKind::DyLoad: {
        int dynOutputs = maxOutput();
        for (int i = 0; i < dynOutputs; ++i)
            tags << "r";
        break;
    }
    }

    emit rowAddRequested(kind, tags);

    // 通知標題列表變更（供 Page3 使用）
    if (kind == LoadKind::Input)
        emit TitleListChanged(LoadKind::Input, TitleList(LoadKind::Input));
    if (kind == LoadKind::Relay)
        emit TitleListChanged(LoadKind::Relay, TitleList(LoadKind::Relay));
    if (kind == LoadKind::Load)
        emit TitleListChanged(LoadKind::Load, TitleList(LoadKind::Load));
    if (kind == LoadKind::DyLoad)
        emit TitleListChanged(LoadKind::DyLoad, TitleList(LoadKind::DyLoad));
}

// 刪除最後一行
void Page2ViewModel::removeRow(LoadKind kind)
{
    emit rowRemoveRequested(kind);

    // 通知標題列表變更
    if (kind == LoadKind::Input)
        emit TitleListChanged(LoadKind::Input, TitleList(LoadKind::Input));
    if (kind == LoadKind::Relay)
        emit TitleListChanged(LoadKind::Relay, TitleList(LoadKind::Relay));
    if (kind == LoadKind::Load)
        emit TitleListChanged(LoadKind::Load, TitleList(LoadKind::Load));
    if (kind == LoadKind::DyLoad)
        emit TitleListChanged(LoadKind::DyLoad, TitleList(LoadKind::DyLoad));
}

// 單元格值變更處理
void Page2ViewModel::cellValueChanged(LoadKind kind,
                                      int row, int col,
                                      const QString &text)
{
    switch (kind) {
    case LoadKind::Input:
        // 更新 Input 標題顯示
        emit inputTitleChanged(row, QString());
        break;

    case LoadKind::Relay: {
        // 只在數據行的 label 變動時更新標題列表
        if (row >= META_ROWS_Relay && col == 0) {
            emit TitleListChanged(LoadKind::Relay, TitleList(LoadKind::Relay));
        }
        break;
    }

    case LoadKind::Load: {
        // row 1: Name 行
        if (row == 1) {
            // emit loadNameListChanged(loadNameList());
        }
        // 數據行的 label 變動
        else if (row >= META_ROWS && col == 0) {
            emit TitleListChanged(LoadKind::Load, TitleList(LoadKind::Load));
        }
        // row 2: Vo 行（影響所有 Power 計算）
        else if (row == 2) {
            for (int r = 0; r < m_model->loadRows.size(); ++r) {
                double p = calcRowPower(r);
                emit powerUpdated(r + META_ROWS, p);
            }
        }
        // 數據行值變動（只影響該行的 Power）
        else if (row >= META_ROWS) {
            int dataRow = row - META_ROWS;
            if (dataRow < m_model->loadRows.size()) {
                double p = calcRowPower(dataRow);
                emit powerUpdated(row, p);
            }
        }
        break;
    }

    case LoadKind::DyLoad: {
        // 只在數據行的 label 變動時更新標題列表
        if (row >= META_ROWS_Dy && col == 0) {
            emit TitleListChanged(LoadKind::DyLoad, TitleList(LoadKind::DyLoad));
        }
        break;
    }

    default:
        break;
    }
}

// 計算指定數據行的功率（Power = Σ(Vo[i] * Value[i])）
double Page2ViewModel::calcRowPower(int dataRow) const
{
    if (dataRow < 0 || dataRow >= m_model->loadRows.size())
        return std::nan("");

    const auto& vo = m_model->loadMeta.vo;
    const auto& rowVals = m_model->loadRows[dataRow].values;
    int N = std::min(int(maxOutput()), std::min(int(vo.size()), int(rowVals.size())));

    // 檢查：如果每一組都是空的，返回 NaN
    bool allEmpty = true;
    for (int i = 0; i < N; ++i) {
        if (!vo[i].trimmed().isEmpty() && !rowVals[i].trimmed().isEmpty()) {
            allEmpty = false;
            break;
        }
    }
    if (allEmpty) return std::nan("");

    // 計算總功率
    double total = 0.0;
    for (int i = 0; i < N; ++i)
        total += vo[i].toDouble() * rowVals[i].toDouble();
    return total;
}

// 廣播所有數據行的功率更新
void Page2ViewModel::broadcastAllPowers()
{
    for (int r = 0; r < m_model->loadRows.size(); ++r) {
        emit powerUpdated(r + META_ROWS, calcRowPower(r));
    }
}

// 獲取 Load 的名稱列表
QStringList Page2ViewModel::loadNameList() const
{
    QStringList result;
    const auto& names = m_model->loadMeta.names;
    int cnt = std::max(int(maxOutput()), int(names.size()));
    for (int i = 0; i < cnt; ++i)
        result << (i < names.size() ? names[i] : QString());
    return result;
}

// XML 序列化
void Page2ViewModel::writeXml(QXmlStreamWriter& writer) const
{
    m_model->writeXml(writer);
}

void Page2ViewModel::loadXml(QXmlStreamReader& reader)
{
    m_model->loadXml(reader);
    refreshUIOutputs();
}

// 刷新 UI 輸出（載入配置後調用）
void Page2ViewModel::refreshUIOutputs()
{
    setMaxOutput(maxOutput());
    setMaxRelayOutput(m_maxRelayOutput);
}

// 獲取指定表格的標題列表（供 Page3 ComboBox 使用）
QStringList Page2ViewModel::TitleList(LoadKind type) const
{
    QStringList titles;
    switch (type) {
    case LoadKind::Input:
        for (const auto& row : m_model->inputRows) {
            if (!row.vin.isEmpty() && !row.frequency.isEmpty() && !row.phase.isEmpty())
                titles << QString("%1/%2/%3").arg(row.vin, row.frequency, row.phase);
            else
                titles << "";
        }
        break;
    case LoadKind::Relay:
        for (const auto& row : m_model->relayRows)
            titles << row.label;
        break;
    case LoadKind::Load:
        for (const auto& row : m_model->loadRows)
            titles << row.label;
        break;
    case LoadKind::DyLoad:
        for (const auto& row : m_model->dynamicRows)
            titles << row.label;
        break;
    }
    return titles;
}

// 獲取最大輸出數（取 names 和 vo 的最大值）
int Page2ViewModel::maxOutput() const {
    return std::max(int(m_model->loadMeta.names.size()), int(m_model->loadMeta.vo.size()));
}

// 配置載入完成回調
void Page2ViewModel::onConfigLoaded()
{
    emit dataChanged();
}

// ========== Model 數據設置（會發送結構變更信號）==========

void Page2ViewModel::setInputRows(const QVector<InputRow>& rows) {
    m_model->inputRows = rows;
    emit inputRowsStructChanged(rows);
    emit TitleListChanged(LoadKind::Input, TitleList(LoadKind::Input));
}

void Page2ViewModel::setRelayRows(const QVector<RelayDataRow>& rows) {
    m_model->relayRows = rows;
    emit relayRowsStructChanged(rows);
    emit TitleListChanged(LoadKind::Relay, TitleList(LoadKind::Relay));
}

void Page2ViewModel::setLoadMeta(const LoadMetaRow& meta) {
    m_model->loadMeta = meta;
    emit loadMetaStructChanged(meta);
    emit TitleListChanged(LoadKind::Load, TitleList(LoadKind::Load));
}

void Page2ViewModel::setLoadRows(const QVector<LoadDataRow>& rows) {
    m_model->loadRows = rows;
    emit loadRowsStructChanged(rows);
}

void Page2ViewModel::setDynamicMeta(const DynamicMetaRow& meta) {
    m_model->dynamicMeta = meta;
    emit dynamicMetaStructChanged(meta);
    emit TitleListChanged(LoadKind::DyLoad, TitleList(LoadKind::DyLoad));
}

void Page2ViewModel::setDynamicRows(const QVector<DynamicDataRow>& rows) {
    m_model->dynamicRows = rows;
    emit dynamicRowsStructChanged(rows);
}
