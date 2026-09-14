#include "conditionpower.h"
#include "page2viewmodel.h"
#include "conditiontextformatter.h"
#include "loadcapabilitycatalog.h"

#include <QVector>

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include "page2model.h"
#include <QPointer>

Page2ViewModel::Page2ViewModel(Page2Model* model, QObject *parent)
    : QObject(parent), m_model(model)
{
    connect(m_model, &Page2Model::configLoaded, this, &Page2ViewModel::onConfigLoaded);
}

// 設置 Load/Dynamic 表格的輸出數量
void Page2ViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0) return;

    const bool outputCountChanged = (m_maxOutput != maxOutput);
    m_maxOutput = maxOutput;

    m_model->resizeLoadOutputs(maxOutput);
    if (!publishConditions()) return;

    QStringList headers    = TableHeaderBuilder::buildIndexHeaders(maxOutput);
    QStringList loadHeaders = headers; loadHeaders << "Power";
    QStringList dynHeaders  = headers; dynHeaders  << "T1~T2 (ms)" << "Power";

    if (outputCountChanged) {
        emit headersChanged(TableKind::Load, loadHeaders);
        emit headersChanged(TableKind::DyLoad, dynHeaders);
    }

    broadcastAllPowers();
    broadcastAllDynamicPowers();

    emit loadMetaStructChanged(m_model->getLoadMeta());
    emit dynamicMetaStructChanged(m_model->getDynamicMeta());
}

// 設置 Relay 表格的輸出數量
void Page2ViewModel::setMaxRelayOutput(int maxRelayOutput)
{
    if (maxRelayOutput <= 0) return;

    const bool outputCountChanged = (m_maxRelayOutput != maxRelayOutput);
    m_maxRelayOutput = maxRelayOutput;

    m_model->resizeRelayOutputs(maxRelayOutput);
    if (!publishConditions()) return;

    QStringList relayHeaders = TableHeaderBuilder::buildRelayHeaders(maxRelayOutput);

    if (outputCountChanged)
        emit headersChanged(TableKind::Relay, relayHeaders);
    emit relayRowsStructChanged(m_model->getRelayRows());
}

// 添加行
void Page2ViewModel::addRow(TableKind kind)
{
    QStringList tags;

    switch (kind) {
    case TableKind::Input:
        tags << "phaseMode" << "double" << "double" << "double";
        break;
    case TableKind::Dc:
        tags << "d";
        break;
    case TableKind::Relay: {
        int relayOutputs = maxRelayOutput();
        for (int i = 0; i < relayOutputs; ++i)
            tags << "combo";
        break;
    }
    case TableKind::Load: {
        int loadOutputs = maxOutput();
        for (int i = 0; i < loadOutputs; ++i)
            tags << "l";
        break;
    }
    case TableKind::DyLoad: {
        int dynOutputs = maxOutput();
        for (int i = 0; i < dynOutputs; ++i)
            tags << "r";
        break;
    }
    }

    emit rowAddRequested(kind, tags);

    // 通知標題列表變更
    emit titleListChanged(kind, TitleList(kind));
}

void Page2ViewModel::removeRow(TableKind kind)
{
    emit rowRemoveRequested(kind);
    emit titleListChanged(kind, TitleList(kind));
}

// 單元格值變更處理
void Page2ViewModel::cellValueChanged(TableKind kind,
                                      int row, int col,
                                      const QString &text)
{
    switch (kind) {
    case TableKind::Input:
        emit inputTitleChanged(row, QString());
        break;

    case TableKind::Dc:
        emit titleListChanged(TableKind::Dc, TitleList(TableKind::Dc));
        break;

    case TableKind::Relay: {
        if (row >= META_ROWS_Relay && col == 0) {
            emit titleListChanged(TableKind::Relay, TitleList(TableKind::Relay));
        }
        break;
    }

    case TableKind::Load: {
        if (row == 2) {
            // Name 行
        }
        else if (row >= META_ROWS && col == 0) {
            emit titleListChanged(TableKind::Load, TitleList(TableKind::Load));
        }
        else if (row == 0 || row == 3) {
            for (int r = 0; r < m_model->getLoadRows().size(); ++r) {
                double p = calcRowPower(r);
                emit powerUpdated(r + META_ROWS, p);
            }
        }
        else if (row >= META_ROWS) {
            int dataRow = row - META_ROWS;
            if (dataRow < m_model->getLoadRows().size()) {
                double p = calcRowPower(dataRow);
                emit powerUpdated(row, p);
            }
        }
        break;
    }

    case TableKind::DyLoad: {
        if (row >= META_ROWS_Dy && col == 0) {
            emit titleListChanged(TableKind::DyLoad, TitleList(TableKind::DyLoad));
        }
        if (row == 1) {
            for (int r = 0; r < m_model->getDynamicRows().size(); ++r)
                emit dynamicPowerUpdated(r + META_ROWS_Dy, calcDynamicRowPower(r));
        } else if (row >= META_ROWS_Dy) {
            int dataRow = row - META_ROWS_Dy;
            if (dataRow < m_model->getDynamicRows().size())
                emit dynamicPowerUpdated(row, calcDynamicRowPower(dataRow));
        }
        break;
    }

    default:
        break;
    }
}

// 計算功率
double Page2ViewModel::calcRowPower(int dataRow) const
{
    if (dataRow < 0 || dataRow >= m_model->getLoadRows().size()) return std::nan("");
    return ConditionPower::load(m_model->getLoadMeta(), m_model->getLoadRows()[dataRow].values, maxOutput());
}

double Page2ViewModel::calcDynamicRowPower(int dataRow) const
{
    if (dataRow < 0 || dataRow >= m_model->getDynamicRows().size()) return std::nan("");
    return ConditionPower::dynamic(m_model->getDynamicMeta(), m_model->getDynamicRows()[dataRow].values, maxOutput());
}

void Page2ViewModel::broadcastAllPowers()
{
    for (int r = 0; r < m_model->getLoadRows().size(); ++r) {
        emit powerUpdated(r + META_ROWS, calcRowPower(r));
    }
}

void Page2ViewModel::broadcastAllDynamicPowers()
{
    for (int r = 0; r < m_model->getDynamicRows().size(); ++r) {
        emit dynamicPowerUpdated(r + META_ROWS_Dy, calcDynamicRowPower(r));
    }
}

QStringList Page2ViewModel::loadNameList() const
{
    QStringList result;
    const auto& names = m_model->getLoadMeta().names;
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
    QPointer<Page2ViewModel> alive(this);
    m_model->loadXml(reader);
    if (!alive || reader.hasError()) return;
    refreshUIOutputs();
}

void Page2ViewModel::refreshUIOutputs()
{
    setMaxOutput(maxOutput());
    setMaxRelayOutput(m_maxRelayOutput);
}

// 獲取標題列表
QStringList Page2ViewModel::TitleList(TableKind type) const
{
    QStringList titles;
    switch (type) {
    case TableKind::Input:
        for (const auto& row : m_model->getInputRows()) {
            titles << ConditionTextFormatter::inputTitle(row);
        }
        break;
    case TableKind::Dc:
        for (const auto& row : m_model->getDcRows())
            titles << row.vin;
        break;
    case TableKind::Relay:
        for (const auto& row : m_model->getRelayRows())
            titles << row.label;
        break;
    case TableKind::Load:
        for (const auto& row : m_model->getLoadRows())
            titles << row.label;
        break;
    case TableKind::DyLoad:
        for (const auto& row : m_model->getDynamicRows())
            titles << row.label;
        break;
    }
    return titles;
}

int Page2ViewModel::maxOutput() const {
    return std::max({int(m_model->getLoadMeta().modes.size()),
                     int(m_model->getLoadMeta().ranges.size()),
                     int(m_model->getLoadMeta().names.size()),
                     int(m_model->getLoadMeta().vo.size()),
                     int(m_model->getLoadMeta().von.size())});
}

QStringList Page2ViewModel::loadRangeOptions(int outputIndex, const QString& baseMode) const
{
    QStringList options{"Auto Range", "No Setting"};

    if (outputIndex <= 0)
        return options;

    for (const auto& inst : m_page1Config.instruments) {
        if (!inst.enabled || inst.type != "Load")
            continue;

        for (const auto& ch : inst.channels) {
            if (ch.index != outputIndex || ch.subModel.trimmed().isEmpty())
                continue;

            const QStringList manualModes =
                LoadCapabilityCatalog::supportedManualModes(ch.subModel.trimmed(), baseMode);
            for (const auto& mode : manualModes) {
                if (!options.contains(mode))
                    options << mode;
            }
            return options;
        }
    }

    return options;
}

QStringList Page2ViewModel::dynamicRangeOptions(int outputIndex) const
{
    QStringList options{"Auto Range", "No Setting"};

    if (outputIndex <= 0)
        return options;

    for (const auto& inst : m_page1Config.instruments) {
        if (!inst.enabled || inst.type != "Load")
            continue;

        for (const auto& ch : inst.channels) {
            if (ch.index != outputIndex || ch.subModel.trimmed().isEmpty())
                continue;

            const QStringList manualModes =
                LoadCapabilityCatalog::supportedDynamicManualModes(ch.subModel.trimmed());
            for (const auto& mode : manualModes) {
                if (!options.contains(mode))
                    options << mode;
            }
            return options;
        }
    }

    return options;
}

// 配置載入完成回調
void Page2ViewModel::onConfigLoaded()
{
    if (!publishConditions()) return;
    QPointer<Page2ViewModel> alive(this);
    const auto revision = m_conditionRevision;
    const auto current = [&] { return alive && revision == m_conditionRevision; };
    emit dataChanged();
    if (!current()) return;

    emit inputRowsStructChanged(m_model->getInputRows());
    if (!current()) return;
    emit dcRowsStructChanged(m_model->getDcRows());
    if (!current()) return;
    emit relayRowsStructChanged(m_model->getRelayRows());
    if (!current()) return;
    emit loadMetaStructChanged(m_model->getLoadMeta());
    if (!current()) return;
    emit loadRowsStructChanged(m_model->getLoadRows());
    if (!current()) return;
    emit dynamicMetaStructChanged(m_model->getDynamicMeta());
    if (!current()) return;
    emit dynamicRowsStructChanged(m_model->getDynamicRows());
    if (!current()) return;
}

void Page2ViewModel::onPage1ConfigChanged(const Page1Config& cfg)
{
    m_page1Config = cfg;
    emit dataChanged();
}

// ========== Model 數據設置 ==========

void Page2ViewModel::setInputRows(const QVector<InputRow>& rows) {
    m_model->setInputRows(rows);
    if (!publishConditions()) return;
    emit inputRowsStructChanged(rows);
    emit titleListChanged(TableKind::Input, TitleList(TableKind::Input));
}

void Page2ViewModel::setDcRows(const QVector<DcRow>& rows) {
    m_model->setDcRows(rows);
    if (!publishConditions()) return;
    emit dcRowsStructChanged(rows);
    emit titleListChanged(TableKind::Dc, TitleList(TableKind::Dc));
}

void Page2ViewModel::setRelayRows(const QVector<RelayDataRow>& rows) {
    m_model->setRelayRows(rows);
    if (!publishConditions()) return;
    emit relayRowsStructChanged(rows);
    emit titleListChanged(TableKind::Relay, TitleList(TableKind::Relay));
}

void Page2ViewModel::setLoadMeta(const LoadMetaRow& meta) {
    m_model->setLoadMeta(meta);
    if (!publishConditions()) return;
    emit loadMetaStructChanged(meta);
    emit titleListChanged(TableKind::Load, TitleList(TableKind::Load));
}

void Page2ViewModel::setLoadRows(const QVector<LoadDataRow>& rows) {
    m_model->setLoadRows(rows);
    if (!publishConditions()) return;
    emit loadRowsStructChanged(rows);
    emit titleListChanged(TableKind::Load, TitleList(TableKind::Load));
}

void Page2ViewModel::setDynamicMeta(const DynamicMetaRow& meta) {
    m_model->setDynamicMeta(meta);
    if (!publishConditions()) return;
    emit dynamicMetaStructChanged(meta);
    emit titleListChanged(TableKind::DyLoad, TitleList(TableKind::DyLoad));
    broadcastAllDynamicPowers();
}

void Page2ViewModel::setDynamicRows(const QVector<DynamicDataRow>& rows) {
    m_model->setDynamicRows(rows);
    if (!publishConditions()) return;
    emit dynamicRowsStructChanged(rows);
    emit titleListChanged(TableKind::DyLoad, TitleList(TableKind::DyLoad));
    broadcastAllDynamicPowers();
}

void Page2ViewModel::validateXml(QXmlStreamReader& reader) const
{
    Page2Model candidate;
    candidate.loadXml(reader);
}

bool Page2ViewModel::publishConditions()
{
    QPointer<Page2ViewModel> alive(this);
    const auto revision = ++m_conditionRevision;
    emit conditionsChanged(m_model->snapshot());
    return alive && revision == m_conditionRevision;
}

void Page2ViewModel::setConditions(const TestConditionSnapshot& snapshot)
{
    // Copy before callbacks; callers may be holding a reference to current data.
    const auto updated = snapshot;
    m_model->setSnapshot(updated);
    if (!publishConditions()) return;
    const auto revision = m_conditionRevision;
    QPointer<Page2ViewModel> alive(this);
    const auto current = [&] { return alive && revision == m_conditionRevision; };
    emit inputRowsStructChanged(updated.inputRows);
    if (!current()) return;
    emit dcRowsStructChanged(updated.dcRows);
    if (!current()) return;
    emit relayRowsStructChanged(updated.relayRows);
    if (!current()) return;
    emit loadMetaStructChanged(updated.loadMeta);
    if (!current()) return;
    emit loadRowsStructChanged(updated.loadRows);
    if (!current()) return;
    emit dynamicMetaStructChanged(updated.dynamicMeta);
    if (!current()) return;
    emit dynamicRowsStructChanged(updated.dynamicRows);
    if (!current()) return;
    for (const auto kind : {TableKind::Input, TableKind::Dc, TableKind::Relay, TableKind::Load, TableKind::DyLoad}) {
        emit titleListChanged(kind, TitleList(kind));
        if (!current()) return;
    }
    broadcastAllPowers();
    if (!current()) return;
    broadcastAllDynamicPowers();
}
