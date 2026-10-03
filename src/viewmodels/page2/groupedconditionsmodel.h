#pragma once
#include "page2viewmodel.h"
#include <QAbstractTableModel>
#include "conditionvalues.h"
#include <algorithm>
#include <cmath>

// Model-backed DC groups and Load/Dynamic metadata. Widgets never own condition data.
class GroupedConditionsModel : public QAbstractTableModel
{
  public:
    enum { ChoicesRole = Qt::UserRole + 1, EditorRole };
    GroupedConditionsModel(Page2ViewModel *vm, TableKind kind, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_vm(vm), m_kind(kind)
    {
        refresh();
        connect(vm, &Page2ViewModel::conditionsChanged, this, [this] { refresh(); });
        connect(vm, &Page2ViewModel::dataChanged, this, [this] { refresh(); });
    }
    TableKind kind() const { return m_kind; }
    int metaRows() const { return m_kind == TableKind::Dc ? 0 : m_kind == TableKind::Load ? 5 : 3; }
    int groupSize() const { return m_kind == TableKind::Dc ? 2 : 1; }
    int recordCount() const { return (rowCount() - metaRows()) / groupSize(); }
    int recordAt(int row) const { return row < metaRows() ? -1 : (row - metaRows()) / groupSize(); }
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_cells.size();
    }
    int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_columns;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return {};
        if (m_kind == TableKind::Dc)
            return QStringList{"Seq", "DC Input", "Parameter", "Index1", "Index2", "Index3"}.value(section);
        if (section == 0)
            return "Seq";
        if (section == 1)
            return "Output";
        if (section == m_columns - 1)
            return "Power";
        if (m_kind == TableKind::DyLoad && section == m_columns - 2)
            return "T1~T2 (ms)";
        return QString("Index%1").arg(section - 1);
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!validIndex(index))
            return {};
        const auto &cell = m_cells[index.row()][index.column()];
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return cell.text;
        if (role == Qt::TextAlignmentRole)
            return int(Qt::AlignCenter);
        if (role == ChoicesRole)
            return cell.choices;
        if (role == EditorRole)
            return cell.editor;
        return {};
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!validIndex(index))
            return Qt::NoItemFlags;
        auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (!m_cells[index.row()][index.column()].editor.isEmpty())
            flags |= Qt::ItemIsEditable;
        return flags;
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
    {
        if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable))
            return false;
        const auto text = value.toString();
        const auto &cell = m_cells[index.row()][index.column()];
        if (!validValue(text, cell.editor, cell.choices))
            return false;
        if (cell.text == text)
            return true;
        auto snapshot = m_vm->conditions();
        const int row = index.row(), col = index.column();
        if (m_kind == TableKind::Dc) {
            normalizeDc(snapshot);
            const int group = row / 2;
            if (col == 1)
                snapshot.dcNames[group] = text;
            else {
                auto &source = dcRows(snapshot, col - 3)[group];
                (row % 2 ? source.currentLimit : source.vin) = text;
            }
        } else if (row < metaRows()) {
            const int output = col - 2;
            auto &values = metadata(snapshot, row);
            values.resize(m_outputs);
            values[output] = text;
            if (m_kind == TableKind::Load && row == 0) {
                snapshot.loadMeta.ranges.resize(m_outputs);
                if (!m_vm->loadRangeOptions(output + 1, text).contains(snapshot.loadMeta.ranges[output]))
                    snapshot.loadMeta.ranges[output] = "Auto Range";
            }
        } else {
            const int record = row - metaRows();
            if (m_kind == TableKind::Load) {
                auto &v = snapshot.loadRows[record];
                if (col == 1)
                    v.label = text;
                else {
                    v.values.resize(m_outputs);
                    v.values[col - 2] = text;
                }
            } else {
                auto &v = snapshot.dynamicRows[record];
                if (col == 1)
                    v.label = text;
                else if (col == m_columns - 2) {
                    snapshot.dynamicMeta.t1t2.resize(snapshot.dynamicRows.size());
                    snapshot.dynamicMeta.t1t2[record] = text;
                } else {
                    v.values.resize(m_outputs);
                    v.values[col - 2] = text;
                }
            }
        }
        m_vm->setConditions(snapshot);
        return true;
    }
    QStringList record(int group) const
    {
        const auto snapshot = m_vm->conditions();
        QStringList values;
        if (group < 0 || group >= recordCount())
            return values;
        if (m_kind == TableKind::Dc) {
            values << snapshot.dcNames.value(group);
            for (int source = 0; source < 3; ++source) {
                const auto v = snapshot.dcSourceRows(source).value(group);
                values << v.vin << v.currentLimit << v.label;
            }
        } else {
            const auto label = m_kind == TableKind::Load ? snapshot.loadRows[group].label
                                                         : snapshot.dynamicRows[group].label;
            const auto row = m_kind == TableKind::Load ? snapshot.loadRows[group].values
                                                       : snapshot.dynamicRows[group].values;
            values << label;
            for (int output = 0; output < m_outputs; ++output)
                values << row.value(output);
            if (m_kind == TableKind::DyLoad)
                values << snapshot.dynamicMeta.t1t2.value(group);
        }
        return values;
    }
    bool insertRecords(int at, const QVector<QStringList> &records)
    {
        if (at < 0 || at > recordCount() || records.isEmpty())
            return false;
        for (const auto &values : records) {
            const int expected =
                m_kind == TableKind::Dc ? 10 : m_outputs + (m_kind == TableKind::DyLoad ? 2 : 1);
            if (values.size() != expected)
                return false;
            for (int i = 1; i < values.size(); ++i) {
                if (m_kind == TableKind::Dc && i % 3 == 0)
                    continue;
                if (!validValue(values[i],
                                m_kind == TableKind::Dc     ? "number"
                                : m_kind == TableKind::Load ? "load"
                                                            : "range",
                                {}))
                    return false;
            }
        }
        auto snapshot = m_vm->conditions();
        if (m_kind == TableKind::Dc)
            normalizeDc(snapshot);
        if (m_kind == TableKind::DyLoad)
            snapshot.dynamicMeta.t1t2.resize(snapshot.dynamicRows.size());
        for (const auto &values : records) {
            if (m_kind == TableKind::Dc) {
                snapshot.dcNames.insert(at, values[0]);
                for (int source = 0; source < 3; ++source)
                    dcRows(snapshot, source)
                        .insert(at, {values[1 + source * 3], values[2 + source * 3], values[3 + source * 3]});
            } else {
                QVector<QString> outputs;
                for (int i = 0; i < m_outputs; ++i)
                    outputs << values[i + 1];
                if (m_kind == TableKind::Load)
                    snapshot.loadRows.insert(at, {values[0], outputs});
                else {
                    snapshot.dynamicRows.insert(at, {values[0], outputs});
                    snapshot.dynamicMeta.t1t2.insert(at, values.last());
                }
            }
            ++at;
        }
        m_vm->setConditions(snapshot);
        return true;
    }
    void append()
    {
        QStringList values;
        const int size = m_kind == TableKind::Dc ? 10 : m_outputs + (m_kind == TableKind::DyLoad ? 2 : 1);
        for (int i = 0; i < size; ++i)
            values << QString();
        insertRecords(recordCount(), {values});
    }
    void removeRecords(QList<int> groups)
    {
        std::sort(groups.begin(), groups.end(), std::greater<int>());
        groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
        auto snapshot = m_vm->conditions();
        if (m_kind == TableKind::Dc)
            normalizeDc(snapshot);
        if (m_kind == TableKind::DyLoad)
            snapshot.dynamicMeta.t1t2.resize(snapshot.dynamicRows.size());
        for (int group : groups) {
            if (group < 0 || group >= recordCount())
                continue;
            if (m_kind == TableKind::Dc) {
                snapshot.dcNames.removeAt(group);
                for (int source = 0; source < 3; ++source)
                    dcRows(snapshot, source).removeAt(group);
            } else if (m_kind == TableKind::Load)
                snapshot.loadRows.removeAt(group);
            else {
                snapshot.dynamicRows.removeAt(group);
                snapshot.dynamicMeta.t1t2.removeAt(group);
            }
        }
        m_vm->setConditions(snapshot);
    }
    void refreshFromSource() { refresh(); }
    static void normalizeDc(TestConditionSnapshot &snapshot)
    {
        const int groups = std::max({snapshot.dcRows.size(), snapshot.dcRows2.size(), snapshot.dcRows3.size(),
                                     snapshot.dcNames.size()});
        snapshot.dcNames.resize(groups);
        for (int source = 0; source < 3; ++source)
            dcRows(snapshot, source).resize(groups);
    }

  private:
    struct Cell {
        QString text, editor;
        QStringList choices;
    };
    static QVector<DcRow> &dcRows(TestConditionSnapshot &s, int source)
    {
        return source == 0 ? s.dcRows : source == 1 ? s.dcRows2 : s.dcRows3;
    }
    QVector<QString> &metadata(TestConditionSnapshot &s, int row) const
    {
        if (m_kind == TableKind::DyLoad)
            return row == 0 ? s.dynamicMeta.ranges : row == 1 ? s.dynamicMeta.vo : s.dynamicMeta.von;
        if (row == 0)
            return s.loadMeta.modes;
        if (row == 1)
            return s.loadMeta.ranges;
        if (row == 2)
            return s.loadMeta.names;
        return row == 3 ? s.loadMeta.vo : s.loadMeta.von;
    }
    bool validIndex(const QModelIndex &index) const
    {
        return index.isValid() && index.model() == this && index.row() < rowCount() &&
               index.column() < m_columns;
    }
    static bool validValue(const QString &text, const QString &editor, const QStringList &choices)
    {
        if (!choices.isEmpty())
            return choices.contains(text);
        if (text.isEmpty() || editor == "text")
            return true;
        if (editor == "number") {
            double value = 0;
            return ConditionValues::finiteNumber(text, value);
        }
        const auto pattern = ConditionValues::editorPattern(editor);
        return pattern.match(text).hasMatch();
    }
    void refresh()
    {
        auto snapshot = m_vm->conditions();
        normalizeDc(snapshot);
        const int outputs = qMax(1, m_vm->maxOutput());
        const int columns = m_kind == TableKind::Dc ? 6 : outputs + (m_kind == TableKind::Load ? 3 : 4);
        const int rows = m_kind == TableKind::Dc
                             ? snapshot.dcRows.size() * 2
                             : metaRows() + (m_kind == TableKind::Load ? snapshot.loadRows.size()
                                                                       : snapshot.dynamicRows.size());
        QVector<QVector<Cell>> cells(rows, QVector<Cell>(columns));
        auto put = [&](int row, int col, QString text, QString editor = {}, QStringList choices = {}) {
            cells[row][col] = {text, editor, choices};
        };
        if (m_kind == TableKind::Dc) {
            for (int row = 0; row < rows; ++row) {
                put(row, 0, QString::number(row / 2 + 1));
                put(row, 1, snapshot.dcNames.value(row / 2), row % 2 ? "" : "text");
                put(row, 2, row % 2 ? "I Limit (A)" : "Vin (V)");
                for (int source = 0; source < 3; ++source) {
                    const auto v = snapshot.dcSourceRows(source).value(row / 2);
                    put(row, source + 3, row % 2 ? v.currentLimit : v.vin, "number");
                }
            }
        } else {
            const bool dynamic = m_kind == TableKind::DyLoad;
            const QStringList labels = dynamic ? QStringList{"Range", "Vo", "Von"}
                                               : QStringList{"Mode", "Range", "Name", "Vo", "Von"};
            for (int row = 0; row < metaRows(); ++row) {
                put(row, 1, labels[row]);
                for (int output = 0; output < outputs; ++output) {
                    auto value = metadata(snapshot, row).value(output);
                    QStringList choices;
                    if (!dynamic && row == 0) {
                        choices = {"CC", "CV"};
                        if (value.isEmpty())
                            value = "CC";
                    }
                    if (row == (dynamic ? 0 : 1)) {
                        choices = dynamic ? m_vm->dynamicRangeOptions(output + 1)
                                          : m_vm->loadRangeOptions(
                                                output + 1, snapshot.loadMeta.modes.value(output, "CC"));
                        if (value.isEmpty())
                            value = "Auto Range";
                        if (!choices.contains(value))
                            choices << value;
                    }
                    put(row, output + 2, value,
                        (!dynamic && row == 2) || !choices.isEmpty() ? "text" : "number", choices);
                }
            }
            for (int row = metaRows(); row < rows; ++row) {
                const int record = row - metaRows();
                put(row, 0, QString::number(record + 1));
                put(row, 1, dynamic ? snapshot.dynamicRows[record].label : snapshot.loadRows[record].label,
                    "text");
                const auto values =
                    dynamic ? snapshot.dynamicRows[record].values : snapshot.loadRows[record].values;
                for (int output = 0; output < outputs; ++output)
                    put(row, output + 2, values.value(output), dynamic ? "range" : "load");
                if (dynamic)
                    put(row, columns - 2, snapshot.dynamicMeta.t1t2.value(record), "range");
                const double power = dynamic ? m_vm->calcDynamicRowPower(record) : m_vm->calcRowPower(record);
                put(row, columns - 1, std::isfinite(power) ? QString::number(power, 'f', 3) : QString());
            }
        }
        if (columns != m_columns || rows != m_cells.size()) {
            beginResetModel();
            m_columns = columns;
            m_outputs = outputs;
            m_cells = std::move(cells);
            endResetModel();
        } else {
            for (int row = 0; row < rows; ++row)
                for (int col = 0; col < columns; ++col) {
                    auto &old = m_cells[row][col];
                    const auto &value = cells[row][col];
                    if (old.text != value.text || old.choices != value.choices) {
                        old = value;
                        emit dataChanged(index(row, col), index(row, col));
                    }
                }
        }
    }
    Page2ViewModel *m_vm;
    TableKind m_kind;
    int m_columns = 0, m_outputs = 1;
    QVector<QVector<Cell>> m_cells;
};
