#pragma once

#include <QString>
#include <QList>
#include <QMetaType>
#include <array>

struct InputRow { QString phaseMode = "1phase", vin, frequency, phase; };
struct DcRow { QString vin; QString currentLimit; QString label; };
using DcGroup = std::array<DcRow, 3>;
inline QString dcInputTitle(const DcRow& row)
{
    QString title = row.label.trimmed();
    if (title.isEmpty() && !row.vin.trimmed().isEmpty()) title = row.vin.trimmed() + " V";
    if (!row.currentLimit.trimmed().isEmpty())
        title += (title.isEmpty() ? QString() : QString(" / ")) + row.currentLimit.trimmed() + " A";
    return title;
}
struct LoadMetaRow { QVector<QString> modes, ranges, names, vo, von; };
struct LoadDataRow { QString label; QVector<QString> values; };
struct DynamicMetaRow { QVector<QString> ranges, vo, von, t1t2; };
struct DynamicDataRow { QString label; QVector<QString> values; };

struct RelayDataRow {
    QString label;
    QVector<QString> values;
};

enum class TableKind { Input, Dc, Relay, Load, DyLoad };

struct TestConditionSnapshot {
    QVector<InputRow> inputRows;
    QVector<DcRow> dcRows;
    QVector<RelayDataRow> relayRows;
    LoadMetaRow loadMeta;
    QVector<LoadDataRow> loadRows;
    DynamicMetaRow dynamicMeta;
    QVector<DynamicDataRow> dynamicRows;
    QVector<DcRow> dcRows2, dcRows3;
    QVector<QString> dcNames;
    const QVector<DcRow>& dcSourceRows(int source) const {
        return source == 0 ? dcRows : source == 1 ? dcRows2 : dcRows3;
    }
};
Q_DECLARE_METATYPE(TestConditionSnapshot)
