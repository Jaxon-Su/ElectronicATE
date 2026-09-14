#pragma once

#include <QString>
#include <QList>
#include <QMetaType>

struct InputRow { QString phaseMode = "1phase", vin, frequency, phase; };
struct DcRow {QString vin;};
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
};
Q_DECLARE_METATYPE(TestConditionSnapshot)
