#include "testsequenceresultsmodel.h"
#include "../../service/report/testsequenceresultrows.h"
#include <algorithm>

int TestSequenceResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : TestSequenceResultRows::Count;
}
QVariant TestSequenceResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= columnCount()) return {};
    const auto value = m_rows[index.row()][index.column()];
    if (role == Qt::ToolTipRole) return m_rows[index.row()][TestSequenceResultRows::Note];
    if (role == Qt::DisplayRole) {
        if (value.metaType().id() == QMetaType::Double) return QString::number(value.toDouble(), 'g', 8);
        return value.toString().isEmpty() ? QVariant(QStringLiteral("—")) : value;
    }
    return {};
}
QVariant TestSequenceResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return {};
    return orientation == Qt::Horizontal ? QVariant(TestSequenceResultRows::headers().value(section)) : QVariant(section + 1);
}
void TestSequenceResultsModel::beginRun()
{
    m_records.clear(); m_filters.clear(); m_taskModes.clear(); rebuild();
}
void TestSequenceResultsModel::updateRecord(const TestSequenceResultRecord &record)
{
    auto found = std::find_if(m_records.begin(), m_records.end(), [&](const auto &old) {
        return old.runId == record.runId && old.taskIndex == record.taskIndex && old.attempt == record.attempt;
    });
    if (found == m_records.end()) m_records.append(record);
    else *found = record;
    rebuild();
}
void TestSequenceResultsModel::selectTask(int uid)
{
    if (m_uid == uid) return;
    m_uid = uid; m_filters.clear(); rebuild();
}
void TestSequenceResultsModel::setSummary(bool summary)
{
    m_taskModes[m_uid] = summary;
    const auto selected = std::find_if(m_records.cbegin(), m_records.cend(),
                                      [&](const auto &r) { return r.dutUid == m_uid; });
    if (selected == m_records.cend() || selected->taskName != "Turn off (descent)")
        m_generalSummary = summary;
    rebuild();
}
bool TestSequenceResultsModel::summary() const
{
    if (m_taskModes.contains(m_uid)) return m_taskModes.value(m_uid);
    const auto selected = std::find_if(m_records.cbegin(), m_records.cend(),
                                      [&](const auto &r) { return r.dutUid == m_uid; });
    return selected != m_records.cend() && selected->taskName == "Turn off (descent)"
        ? false : m_generalSummary;
}
void TestSequenceResultsModel::setFilter(int column, const QString &value)
{
    if (m_filters.value(column) == value) return;
    m_filters[column] = value; rebuild();
}
QStringList TestSequenceResultsModel::options(int column) const
{
    QStringList values;
    for (const auto &row : m_allRows) {
        const auto text = row.value(column).toString();
        if (!text.isEmpty() && !values.contains(text)) values.append(text);
    }
    return values;
}
void TestSequenceResultsModel::rebuild()
{
    beginResetModel();
    m_allRows.clear(); m_rows.clear();
    for (const auto &record : m_records)
        if (m_uid >= 0 && record.dutUid == m_uid) m_allRows += TestSequenceResultRows::rows(record, summary());
    for (auto it = m_filters.begin(); it != m_filters.end(); ++it)
        if (!options(it.key()).contains(it.value())) it.value().clear();
    for (const auto &row : m_allRows) {
        bool keep = true;
        for (auto it = m_filters.cbegin(); it != m_filters.cend(); ++it)
            keep &= it.value().isEmpty() || row.value(it.key()).toString() == it.value();
        if (keep) m_rows.append(row);
    }
    endResetModel();
    emit summaryChanged(summary());
}
