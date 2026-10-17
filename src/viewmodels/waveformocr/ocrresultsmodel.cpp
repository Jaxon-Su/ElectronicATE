#include "ocrresultsmodel.h"
#include "../../service/waveformocr/waveformocrparser.h"
#include <QColor>
#include <QFileInfo>

QVariant OcrResultsModel::data(const QModelIndex &index, int role) const
{
    const auto *r = row(index.row());
    if (index.model() != this || !r || index.column() < 0 || index.column() >= Count)
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
            return "目前數值: " + r->values[col - Max].raw + "\n雙擊或按 F2 修改，例如 5.1 V、-185.3 mA";
        if (col == Channel || col == Scope)
            return "選填資訊；未辨識到不影響數值與匯出。雙擊可修改。";
        return WaveformOcr::issues(*r).join("\n");
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    if (col >= Max && col <= Mean)
        return role == Qt::EditRole ? (r->values[col - Max].number
                                          ? WaveformOcr::displayValue(r->values[col - Max])
                                          : r->values[col - Max].raw)
                                    : WaveformOcr::displayValue(r->values[col - Max]);
    switch (col)
    {
    case Image:
        return (r->folder.isEmpty() || r->folder == "." ? QString() : r->folder + "/") +
               QFileInfo(r->imagePath).fileName();
    case Channel:
        return role == Qt::DisplayRole && r->channel.isEmpty() ? QString("—") : r->channel;
    case Scope:
        return r->scope == "unspecified" || r->scope.isEmpty()
                   ? (role == Qt::DisplayRole ? QString("—") : QString()) : r->scope;
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
    static const QStringList names{"圖片", "通道（選填）", "統計範圍（選填）", "Max",  "Min",
                                   "RMS",  "Mean", "已核對",   "狀態", "數值問題"};
    return section >= 0 && section < names.size() ? names[section] : QVariant();
}
Qt::ItemFlags OcrResultsModel::flags(const QModelIndex &index) const
{
    if (index.model() != this || !row(index.row()) || index.column() < 0 || index.column() >= Count)
        return Qt::NoItemFlags;
    auto result = QAbstractTableModel::flags(index);
    if (!index.isValid() || m_locked)
        return result;
    if (index.column() == Review)
        result |= Qt::ItemIsUserCheckable;
    if ((index.column() >= Max && index.column() <= Mean) ||
        index.column() == Channel || index.column() == Scope)
        result |= Qt::ItemIsEditable;
    return result;
}
bool OcrResultsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (m_locked || index.model() != this || !index.isValid() || index.column() < 0 ||
        index.column() >= Count || !row(index.row()))
        return false;
    auto &r = m_rows[index.row()];
    if (role == Qt::CheckStateRole && index.column() == Review)
    {
        if (value.toInt() != Qt::Checked && value.toInt() != Qt::Unchecked)
            return false;
        const bool reviewed = value.toInt() == Qt::Checked;
        if (r.reviewed == reviewed)
            return true;
        r.reviewed = reviewed;
    }
    else if (role == Qt::EditRole && index.column() >= Max && index.column() <= Mean)
    {
        const int field = index.column() - Max;
        const QString text = value.toString().trimmed();
        if (text == data(index, Qt::EditRole).toString())
            return true;
        auto parsed = WaveformOcr::parseValue(text);
        // A number-only edit keeps its existing unit, or the one unambiguous
        // unit used elsewhere in this row. An unknown unit must stay missing.
        if (parsed.issue == "缺少單位")
        {
            QString unit = r.values[field].unit;
            if (unit.isEmpty())
            {
                QStringList units;
                for (const auto &candidate : r.values)
                    if (candidate.number && !units.contains(candidate.unit))
                        units << candidate.unit;
                if (units.size() == 1)
                    unit = units.first();
            }
            if (!unit.isEmpty())
                parsed = WaveformOcr::parseValue(text + " " + unit);
        }
        const auto &previous = r.values[field];
        if ((parsed.number && previous.number && parsed.number == previous.number && parsed.unit == previous.unit) ||
            (!parsed.number && !previous.number && parsed.raw == previous.raw && parsed.issue == previous.issue))
            return true;
        r.values[field] = parsed;
        r.modified = true;
        r.reviewed = false;
    }
    else if (role == Qt::EditRole && (index.column() == Channel || index.column() == Scope))
    {
        auto &metadata = index.column() == Channel ? r.channel : r.scope;
        const auto text = value.toString().trimmed();
        if (metadata == text || (index.column() == Scope && metadata == "unspecified" && text.isEmpty()))
            return true;
        metadata = text;
        r.reviewed = false;
    }
    else
        return false;
    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), Count - 1));
    emit resultEdited();
    return true;
}
void OcrResultsModel::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    if (!m_rows.isEmpty())
        emit dataChanged(index(0, 0), index(m_rows.size() - 1, Count - 1));
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
