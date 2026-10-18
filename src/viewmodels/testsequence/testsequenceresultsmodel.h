#pragma once
#include "testsequenceresultrecord.h"
#include <QAbstractTableModel>

// Current Run, including all attempts. Selection uses the stable DUT uid.
class TestSequenceResultsModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum FilterColumn { ChannelFilter = 3, AttemptFilter = 11, LoadFilter = 12, DirectionFilter = 13 };
    explicit TestSequenceResultsModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_rows.size(); }
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void beginRun();
    void updateRecord(const TestSequenceResultRecord &record);
    void selectTask(int uid);
    void setSummary(bool summary);
    void setFilter(int column, const QString &value);
    QStringList options(int column) const;
    QString filterValue(int column) const { return m_filters.value(column); }
private:
    void rebuild();
    QVector<TestSequenceResultRecord> m_records;
    QVector<QVariantList> m_allRows, m_rows;
    QMap<int, QString> m_filters;
    int m_uid = -1;
    bool m_summary = true;
};
