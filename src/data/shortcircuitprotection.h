#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

// Runtime-only handoff; never persisted to XML as a claimed hardware state.
struct ShortCircuitProtection {
    QVector<QString> shortMask, dischargeMask;
    int powerOffMs = 300;
    int dischargeMs = 10000;
    bool shortMayBeOn = false;
    bool active() const { return !shortMask.isEmpty(); }
};
Q_DECLARE_METATYPE(ShortCircuitProtection)
