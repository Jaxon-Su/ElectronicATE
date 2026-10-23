#pragma once
#include "ioscilloscopemeasurestrategy.h"
#include "verticalscaleoptions.h"
#include <functional>

enum class TransientKind { TurnOn, TurnOff, TurnOnThenShort, ShortThenTurnOn };
struct TransientContext {
    OscCaptureObserver captureRecord;
    std::function<bool(bool, QString &)> power;
    std::function<bool(bool, QString &)> load;
    // One combined relay state preserves short while discharge is released.
    std::function<bool(bool, bool, QString &)> relays;
    std::function<void(const QString &)> log;
    QString target = "BOTH";
    QString edge = "AUTO";
    int timeoutMs = 10000;
    int settleMs = 5000;
    int dischargeMs = 10000;
    int powerOffMs = 300;
    // Runtime uses 3000 ms; internal time fixtures may shorten this like settle/discharge.
    // This is not a Dialog/XML setting.
    int singleProcessingMs = 3000;
    int phaseTimeoutMs = 900000;
    int maxIterations = 100;
    int trialsPerLevel = 3;
    int descentChannel = 0;
    bool descentChannelEnabled = true;
    QVector<double> descentLevels;
    VerticalScaleOptions verticalScale;
};
OscMeasureResult runTransientStrategy(TransientKind kind, const TransientContext &context,
                                      IScopeMeasurement *scope, QAtomicInt &stop);
