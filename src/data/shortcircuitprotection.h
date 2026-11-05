#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

struct ShortCircuitExitDelays {
    int shortToDischargeMs = 1000;
    int dischargeToReleaseMs = 1000;
    int afterReleaseMs = 2000;
    bool valid() const { return shortToDischargeMs >= 0 && dischargeToReleaseMs >= 0 && afterReleaseMs >= 0; }
};

// Runtime-only handoff; never persisted to XML as a claimed hardware state.
struct ShortCircuitProtection {
    QVector<QString> shortMask, dischargeMask;
    int powerOffMs = 300;
    int dischargeMs = 10000;
    bool shortMayBeOn = false;
    ShortCircuitExitDelays exitDelays;
    bool active() const { return !shortMask.isEmpty(); }
};
Q_DECLARE_METATYPE(ShortCircuitProtection)
