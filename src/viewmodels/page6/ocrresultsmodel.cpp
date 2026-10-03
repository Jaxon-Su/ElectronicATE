#include "ocrresultsmodel.h"
#include "../../service/waveformocr/waveformocrparser.h"
#include <QColor>
#include <QFileInfo>

QVariant OcrResultsModel::data(const QModelIndex &index, int role) const
{
    const auto *r = row(index.row());
    if (!r || index.column() < 0 || index.column() >= Count)
        return {};
    const int col = index.column();
    if (role == Qt::CheckStateRole && col == Review)
        return r->reviewed ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::BackgroundRole && col >= Max && col <= Mean && !r->values[col - Max].number)
        return QColor("#fff0cf");
    if (role == Qt::ToolTipRole)
    {
        if (col == Image)
            return r->imagePath;
        if (col >= Max && col <= Mean)
            return "原始文字: " + r->values[col - Max].raw + "\n唯讀；請對照原圖核對";
        return WaveformOcr::issues(*r).join("\n");
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    if (col >= Max && col <= Mean)
        return role == Qt::EditRole ? r->values[col - Max].raw
                                    : WaveformOcr::displayValue(r->values[col - Max]);
    switch (col)
    {
    case Image:
        return QFileInfo(r->imagePath).fileName();
    case Channel:
        return r->channel;
    case Scope:
        return r->scope;
    case Status:
        return WaveformOcr::status(*r);
    case Issues:
        return WaveformOcr::issues(*r).join("; ");
    default:
        return {};
    }
}
QVariant OcrResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return section + 1;
    static const QStringList names{"圖片", "通道", "統計範圍", "Max",  "Min",
                                   "RMS",  "Mean", "已核對",   "狀態", "待確認事項"};
    return section >= 0 && section < names.size() ? names[section] : QVariant();
}
Qt::ItemFlags OcrResultsModel::flags(const QModelIndex &index) const
{
    auto result = QAbstractTableModel::flags(index);
    if (!index.isValid() || m_locked)
        return result;
    if (index.column() == Review)
        result |= Qt::ItemIsUserCheckable;
    return result;
}
bool OcrResultsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (m_locked || !row(index.row()))
        return false;
    auto &r = m_rows[index.row()];
    if (role == Qt::CheckStateRole && index.column() == Review)
        r.reviewed = value.toInt() == Qt::Checked;
    else
        return false;
    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), Count - 1));
    return true;
}
void OcrResultsModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}
void OcrResultsModel::append(const QVector<WaveformOcr::Row> &rows)
{
    if (rows.isEmpty())
        return;
    beginInsertRows({}, m_rows.size(), m_rows.size() + rows.size() - 1);
    m_rows += rows;
    endInsertRows();
}
