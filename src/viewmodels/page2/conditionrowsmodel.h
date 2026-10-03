#pragma once
#include "conditionvalues.h"
#include "page2viewmodel.h"
#include "conditiontextformatter.h"
#include <QAbstractTableModel>
#include <QSet>
#include <algorithm>
#include <cmath>

// Presentation model over the authoritative condition snapshot. AC/Relay edits
// commit one transaction; derived labels are never copied back from widgets.
class ConditionRowsModel : public QAbstractTableModel
{
  public:
    enum { ChoicesRole = Qt::UserRole + 1 };
    ConditionRowsModel(Page2ViewModel *vm, TableKind kind, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_vm(vm), m_kind(kind)
    {
        refresh();
        connect(vm, &Page2ViewModel::conditionsChanged, this, [this] { refresh(); });
    }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_rows; }
    int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_columns;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return {};
        if (m_kind == TableKind::Input)
            return QStringList{"Seq", "AC Input", "Mode", "Vin", "Frequency", "Phase"}.value(section);
        return section == 0   ? QString("Seq")
               : section == 1 ? QString("Relay")
                              : QString("Index%1").arg(section - 1);
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.model() != this || index.row() >= m_rows || index.column() >= m_columns)
            return {};
        const int row = index.row(), column = index.column();
        if (role == Qt::TextAlignmentRole)
            return int(Qt::AlignCenter);
        if (role == ChoicesRole) {
            if (m_kind == TableKind::Input && column == 2)
                return QStringList{"1phase", "3phase"};
            if (m_kind == TableKind::Relay && column >= 2)
                return QStringList{"off", "on"};
            return {};
        }
        if (role != Qt::DisplayRole && role != Qt::EditRole)
            return {};
        return m_cells.value(row).value(column);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid() || index.model() != this || index.row() >= m_rows || index.column() >= m_columns)
            return Qt::NoItemFlags;
        auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (index.column() >= firstEditableColumn())
            flags |= Qt::ItemIsEditable;
        return flags;
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
    {
        if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable) || index.row() >= m_rows)
            return false;
        if (data(index, Qt::EditRole).toString() == value.toString())
            return true;
        auto record = editableRow(index.row());
        record[index.column() - firstEditableColumn()] = value.toString();
        if (!valid(record))
            return false;
        auto snapshot = m_vm->conditions();
        assign(snapshot, index.row(), record, false);
        m_vm->setConditions(snapshot);
        return true;
    }
    QStringList editableRow(int row) const
    {
        QStringList values;
        for (int column = firstEditableColumn(); column < m_columns; ++column)
            values << data(index(row, column), Qt::EditRole).toString();
        return values;
    }
    void refreshFromSource() { refresh(); }
    int firstEditableColumn() const { return m_kind == TableKind::Input ? 2 : 1; }
    bool insertRecords(int row, const QVector<QStringList> &records)
    {
        if (row < 0 || row > m_rows || records.isEmpty())
            return false;
        for (const auto &record : records)
            if (!valid(record))
                return false;
        auto snapshot = m_vm->conditions();
        for (const auto &record : records)
            assign(snapshot, row++, record, true);
        m_vm->setConditions(snapshot);
        return true;
    }
    void append()
    {
        QStringList values;
        if (m_kind == TableKind::Input)
            values = {"1phase", "", "", ""};
        else {
            values << "";
            for (int i = 2; i < m_columns; ++i)
                values << "off";
        }
        insertRecords(m_rows, {values});
    }
    void removeRecords(QList<int> rows)
    {
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
        auto snapshot = m_vm->conditions();
        for (int row : rows) {
            if (row < 0 || row >= m_rows)
                continue;
            if (m_kind == TableKind::Input)
                snapshot.inputRows.removeAt(row);
            else
                snapshot.relayRows.removeAt(row);
        }
        m_vm->setConditions(snapshot);
    }

  private:
    void refresh()
    {
        const int rows = m_kind == TableKind::Input ? m_vm->inputRows().size() : m_vm->relayRows().size();
        const int columns = m_kind == TableKind::Input ? 6 : m_vm->maxRelayOutput() + 2;
        QVector<QVector<QVariant>> cells;
        for (int row = 0; row < rows; ++row) {
            QVector<QVariant> values{row + 1};
            if (m_kind == TableKind::Input) {
                const auto &v = m_vm->inputRows()[row];
                values << ConditionTextFormatter::inputTitle(v)
                       << (v.phaseMode.isEmpty() ? QString("1phase") : v.phaseMode) << v.vin << v.frequency
                       << v.phase;
            } else {
                const auto &v = m_vm->relayRows()[row];
                values << v.label;
                for (int column = 2; column < columns; ++column)
                    values << v.values.value(column - 2, "off");
            }
            cells << values;
        }
        if (rows != m_rows || columns != m_columns) {
            beginResetModel();
            m_rows = rows;
            m_columns = columns;
            m_cells = std::move(cells);
            endResetModel();
        } else {
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column)
                    if (m_cells[row][column] != cells[row][column]) {
                        m_cells[row][column] = cells[row][column];
                        emit dataChanged(index(row, column), index(row, column),
                                         {Qt::DisplayRole, Qt::EditRole});
                    }
        }
    }

    bool valid(const QStringList &record) const
    {
        if (record.size() != m_columns - firstEditableColumn())
            return false;
        if (m_kind == TableKind::Relay) {
            for (int i = 1; i < record.size(); ++i)
                if (record[i] != "off" && record[i] != "on")
                    return false;
        } else {
            if (!QStringList{"1phase", "3phase"}.contains(record[0]))
                return false;
            for (int i = 1; i < record.size(); ++i) {
                if (record[i].isEmpty())
                    continue;
                double value = 0;
                if (!ConditionValues::finiteNumber(record[i], value))
                    return false;
            }
        }
        return true;
    }
    void assign(TestConditionSnapshot &snapshot, int row, const QStringList &record, bool insert) const
    {
        if (m_kind == TableKind::Input) {
            InputRow value{record[0], record[1], record[2], record[3]};
            if (insert)
                snapshot.inputRows.insert(row, value);
            else
                snapshot.inputRows[row] = value;
        } else {
            RelayDataRow value;
            value.label = record[0];
            for (int i = 1; i < record.size(); ++i)
                value.values << record[i];
            if (insert)
                snapshot.relayRows.insert(row, value);
            else
                snapshot.relayRows[row] = value;
        }
    }
    Page2ViewModel *m_vm;
    TableKind m_kind;
    int m_rows = 0, m_columns = 0;
    QVector<QVector<QVariant>> m_cells;
};
