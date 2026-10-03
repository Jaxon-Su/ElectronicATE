#pragma once
#include "../../data/waveformocrresult.h"
#include <QAbstractTableModel>

class OcrResultsModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    enum Column
    {
        Image,
        Channel,
        Scope,
        Max,
        Min,
        Rms,
        Mean,
        Review,
        Status,
        Issues,
        Count
    };
    explicit OcrResultsModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : Count; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void clear();
    void append(const QVector<WaveformOcr::Row> &rows);
    void setLocked(bool locked) { m_locked = locked; }
    const QVector<WaveformOcr::Row> &rows() const { return m_rows; }
    const WaveformOcr::Row *row(int index) const
    {
        return index >= 0 && index < m_rows.size() ? &m_rows[index] : nullptr;
    }

  private:
    QVector<WaveformOcr::Row> m_rows;
    bool m_locked = false;
};
