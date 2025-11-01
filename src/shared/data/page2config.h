#pragma once

#include <QString>
#include <QList>
#include <QMetaType>

struct InputRow { QString vin, frequency, phase; };
struct LoadMetaRow { QVector<QString> modes, names, vo, von, riseSlopeCCH, fallSlopeCCH,riseSlopeCCL,fallSlopeCCL; };
struct LoadDataRow { QString label; QVector<QString> values; };
struct DynamicMetaRow { QVector<QString> vo,von, riseSlopeCCDH,fallSlopeCCDH,riseSlopeCCDL,fallSlopeCCDL,t1t2; };
struct DynamicDataRow { QString label; QVector<QString> values; };

struct RelayDataRow {
    QString label;
    QVector<QString> values;
};

enum class LoadKind { Input, Relay, Load, DyLoad };
