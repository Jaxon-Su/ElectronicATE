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
              QAtomicInt &stopFlag, bool shortProtection, bool diagnostics = false);
    Execution(const Execution &) = delete;
    Execution &operator=(const Execution &) = delete;

    void log(const QString &message);
    // Turn off diagnostics observe the existing phase clock; never restart it or query hardware.
    bool diagnosticsEnabled() const { return diagnostics; }
    void logProgress(const QString &message);
    void diagnostic(const QString &message);
    void diagnosticStage(const QString &name, const QString &details = {});
    void diagnosticPhase(bool maximum);
    void diagnosticSettings();
    void diagnosticDelay(int ms, const QString &name);
    void check();
    void checked(bool ok, const QString &error);
    void configure(const std::function<bool(QString &)> &operation);
    void stopScope(const QString &diagnosticName);
    void power(bool on);
    void relays(bool shorted, bool discharge);
    void requireShortExit();
    void delay(int ms);
    void waitAfterSingle();
    // Baseline never applies the completed-record short-exit callback.
    OscilloscopeAcquisition::AcquisitionResult waitForBaseline();
    // Short events exit as soon as the record completes, before Capture/reads.
    OscilloscopeAcquisition::AcquisitionResult waitForEvent();
    void requireReady();
    // False means a natural pre-input acquisition was completed and diagnosed.
    // The caller may move Level and rearm while leaving the discharged input OFF.
    // Short events complete their protected exit before diagnosis/recovery.
    bool waitForPowerOnReady();
    // NoTrigger means READY and still acquiring; completed records belong to
    // the powered baseline, before short. The caller diagnoses them after exit.
    OscilloscopeAcquisition::AcquisitionResult waitForShortReady();
    OscMeasureResult finish();

    const TransientContext &ctx;
    IScopeMeasurement *scope;
    QAtomicInt &stop;
    OscMeasureResult result;
    QMap<int, OscChannelMeasure> channels;
    QElapsedTimer phase;
    bool touched = false;

private:
    const bool diagnostics;
    QString phaseName = "not-started";
    QString diagnosticStageName = "Initialization";
    enum class ReadinessPurpose { Event, PowerOn, Short };
    OscilloscopeAcquisition::AcquisitionResult waitForReadiness(ReadinessPurpose purpose);
    OscilloscopeAcquisition::AcquisitionResult waitForRecord(bool exitShortOnCompletion);
    bool exitShort();
    const bool shortTest;
    bool shortMayBeOn;
    bool shortExitNeeded = true;
    bool shortExitFailed = false;
    QString shortExitError;
};

} // namespace TransientDetail
