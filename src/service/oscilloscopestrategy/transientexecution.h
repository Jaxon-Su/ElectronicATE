#pragma once
#include "transientstrategy.h"
#include "oscilloscopeacquisition.h"
#include <QElapsedTimer>
#include <QMap>

namespace TransientDetail {

// One instance per run, shared by search and event flow. No destructor performs IO.
// In particular, the retained-short latch must survive every direction/level/trial.
class Execution {
public:
    Execution(const TransientContext &context, IScopeMeasurement *instrument,
              QAtomicInt &stopFlag, bool shortProtection);
    Execution(const Execution &) = delete;
    Execution &operator=(const Execution &) = delete;

    void log(const QString &message);
    void check();
    void checked(bool ok, const QString &error);
    void configure(const std::function<bool(QString &)> &operation);
    void power(bool on);
    void relays(bool shorted, bool discharge);
    void requireShortExit();
    void delay(int ms);
    // Baseline never applies the completed-record short-exit callback.
    OscilloscopeAcquisition::AcquisitionResult waitForBaseline();
    // Short events exit as soon as the record completes, before Capture/reads.
    OscilloscopeAcquisition::AcquisitionResult waitForEvent();
    void requireReady();
    OscMeasureResult finish();

    const TransientContext &ctx;
    IScopeMeasurement *scope;
    QAtomicInt &stop;
    OscMeasureResult result;
    QMap<int, OscChannelMeasure> channels;
    QElapsedTimer phase;
    bool touched = false;

private:
    OscilloscopeAcquisition::AcquisitionResult waitForRecord(bool exitShortOnCompletion);
    bool exitShort();
    const bool shortTest;
    bool shortMayBeOn;
    bool shortExitNeeded = true;
    bool shortExitFailed = false;
    QString shortExitError;
};

} // namespace TransientDetail
