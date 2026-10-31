#include "manualscopecontrol.h"
#include "scopesessioncoordinator.h"
#include "oscilloscopefactory.h"
#include "capturelease.h"
#include "tasksettings.h"
#include "testsequencesettingsvalidation.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <iostream>
#include <atomic>
#include <stdexcept>
#include <cmath>

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
template<class Predicate> void until(Predicate predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 4000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(predicate(), "asynchronous condition timed out");
}

class ScriptedTransport : public ICommunication {
public:
    bool open() override { opened = true; return true; }
    void close() override { opened = false; }
    bool isOpen() const override { return opened; }
    QString lastError() const override { return "injected transport error"; }
    int write(const QByteArray &data) override
    {
        consumed = false;
        last = QString::fromUtf8(data).trimmed();
        writes.append(last);
        if (last == cancelAfter && cancellation) cancellation->storeRelease(1);
        return last == failWrite ? -1 : data.size();
    }
    int read(QByteArray &data, int) override
    {
        if (consumed) { data.clear(); return 0; }
        consumed = true;
        if (!queued[last].isEmpty()) data = queued[last].takeFirst();
        else if (responses.contains(last)) data = responses.value(last);
        else if (last == "*ESR?") data = "0";
        else if (last == "TRIGger:A:EDGE:SOUrce?") data = source;
        else if (last == "DISplay:GLObal:CH2:STATE?") data = "1";
        else if (last == "CH2:SCAle?") data = scale;
        else return -1;
        return data.size();
    }
    QMap<QString,QList<QByteArray>> queued;
    QMap<QString,QByteArray> responses;
    QStringList writes;
    QString last, failWrite, cancelAfter;
    QByteArray source = "CH2", scale = "2.5";
    QAtomicInt *cancellation = nullptr;
    bool opened = false, consumed = false;
};

void driverContract()
{
    ScriptedTransport transport;
    std::unique_ptr<Oscilloscope> scope(OscilloscopeFactory::createOscilloscope("MSO44B", &transport));
    require(bool(scope), "production factory rejected MSO44B");
    scope->connect();
    QAtomicInt stop{0};
    QString error;
    require(scope->prepareSteadyAcquisition(stop, error), "production startup failed");
    const QStringList expected{
        "*ESR?", "TRIGger:A:MODe AUTO", "*ESR?",
        "TRIGger:A:EDGE:SOUrce?", "DISplay:GLObal:CH2:STATE?", "CH2:SCAle?",
        "*ESR?", "TRIGger:A:TYPe EDGE", "*ESR?",
        "*ESR?", "TRIGger:A:EDGE:SLOpe RISE", "*ESR?",
        "*ESR?", "TRIGger:A:LEVel:CH2 0.1", "*ESR?",
        "*ESR?", "ACQuire:STOPAfter RUNSTop", "*ESR?",
        "*ESR?", "ACQuire:STATE RUN", "*ESR?"};
    require(transport.writes == expected, "real SCPI sequence or CH2 scale-derived level changed");
    transport.writes.clear();
    require(scope->prepareSteadyAcquisition(stop, error, false), "Capture startup configuration failed");
    require(transport.writes.contains("ACQuire:STATE STOP") &&
                transport.writes.indexOf("ACQuire:STATE STOP") < transport.writes.indexOf("TRIGger:A:MODe AUTO") &&
                !transport.writes.contains("ACQuire:STATE RUN"),
            "Capture startup ran unobserved acquisitions");
    transport.writes.clear();
    transport.failWrite = "ACQuire:STATE STOP";
    require(!scope->prepareSteadyAcquisition(stop, error, false) &&
                !transport.writes.contains("TRIGger:A:MODe AUTO"), "Capture startup ignored STOP failure");
    transport.writes.clear();
    transport.failWrite = "TRIGger:A:TYPe EDGE";
    require(!scope->prepareSteadyAcquisition(stop, error) && !error.isEmpty(), "failed write accepted");
    require(transport.writes.last() == transport.failWrite && !transport.writes.contains("ACQuire:STATE RUN"),
            "startup continued after failed write");
    transport.failWrite.clear();
    transport.writes.clear();
    transport.scale = "invalid";
    require(!scope->prepareSteadyAcquisition(stop, error) && !error.isEmpty(), "invalid scale accepted");
    require(!transport.writes.contains("ACQuire:STATE RUN"), "run sent after invalid scale");
    transport.scale = "2.5";
    require(scope->prepareSteadyAcquisition(stop, error) && error.isEmpty(), "successful retry retained stale error");
    transport.writes.clear();
    transport.cancelAfter = "TRIGger:A:MODe AUTO";
    transport.cancellation = &stop;
    require(!scope->prepareSteadyAcquisition(stop, error), "cancelled startup succeeded");
    require(!transport.writes.contains("TRIGger:A:EDGE:SOUrce?"), "queries continued after cancellation");
}

void measurementDriverContract()
{
    ScriptedTransport transport;
    std::unique_ptr<Oscilloscope> concrete(OscilloscopeFactory::createOscilloscope("MSO44B", &transport));
    require(bool(concrete), "scope factory failed");
    concrete->connect();
    require(concrete->isConnected(), "scope connect failed");
    IScopeMeasurement &scope = *concrete;
    QString error;
    // Start from STOP and verify that the real driver resumes Single, with checked writes.
    const QStringList singleCommands{"ACQuire:STATE STOP", "ACQuire:SEQuence:MODe NUMACQS",
        "ACQuire:SEQuence:NUMSEQuence 1", "ACQuire:STOPAfter SEQuence", "ACQuire:STATE RUN"};
    transport.responses["ACQuire:SEQuence:NUMSEQuence?"] = "1";
    transport.responses["ACQuire:SEQuence:MODe?"] = "NUMACQS";
    transport.responses["ACQuire:STOPAfter?"] = "SEQUENCE";
    transport.writes.clear();
    require(scope.beginSingleAcquisition(), "Single did not start from STOP");
    QStringList singleTrace;
    for (const auto& command : singleCommands) {
        if (command == "ACQuire:STATE RUN")
            singleTrace << "ACQuire:SEQuence:MODe?" << "ACQuire:SEQuence:NUMSEQuence?" << "ACQuire:STOPAfter?";
        singleTrace << "*ESR?" << command << "*ESR?";
    }
    require(transport.writes == singleTrace, "Single did not send checked STOP -> Sequence -> RUN");
    for (const auto& command : singleCommands) {
        transport.writes.clear(); transport.failWrite = command;
        require(!scope.beginSingleAcquisition() && transport.writes.last() == command,
                "Single continued after a failed command");
    }
    transport.failWrite.clear();
    for (int rejected = 0; rejected < singleCommands.size(); ++rejected) {
        QList<QByteArray> esr;
        for (int i = 0; i < rejected; ++i) esr << "0" << "0";
        esr << "0" << "32";
        transport.queued["*ESR?"] = esr; transport.writes.clear();
        require(!scope.beginSingleAcquisition(), "Single hid device rejection");
        if (rejected < singleCommands.size()-1) require(!transport.writes.contains("ACQuire:STATE RUN"), "RUN sent after rejected setup");
    }
    transport.responses["ACQuire:SEQuence:NUMSEQuence?"] = "3";
    transport.writes.clear();
    require(!scope.beginSingleAcquisition() && !transport.writes.contains("ACQuire:STATE RUN"),
            "mismatched Single sequence count armed the instrument");
    transport.responses["ACQuire:SEQuence:NUMSEQuence?"] = "1";
    transport.responses["ACQuire:SEQuence:MODe?"] = "TIME";
    transport.writes.clear();
    require(!scope.beginSingleAcquisition() && !transport.writes.contains("ACQuire:STATE RUN"),
            "time-based sequence armed despite the requested one record");
    transport.responses["ACQuire:SEQuence:MODe?"] = "NUMACQS";
    transport.writes.clear();
    require(scope.selectEdgeTrigger(error) && scope.setEdgeSlope(ScopeTriggerSlope::Rise,error)
        && scope.setEdgeSlope(ScopeTriggerSlope::Fall,error)
        && scope.setChannelTriggerLevel(2,1.234567890123456,error)
        && scope.setTriggerMode(ScopeTriggerMode::Auto,error)
        && scope.setTriggerMode(ScopeTriggerMode::Normal,error)
        && scope.setVerticalScale(2,.25,error) && scope.stopAcquisition(error), "semantic write failed");
    QStringList expected;
    for(const QString command : {"TRIGger:A:TYPe EDGE", "TRIGger:A:EDGE:SLOpe RISE", "TRIGger:A:EDGE:SLOpe FALL",
        "TRIGger:A:LEVel:CH2 1.234567890123456", "TRIGger:A:MODe AUTO", "TRIGger:A:MODe NORMAL",
        "CH2:SCAle 0.25", "ACQuire:STATE STOP"}) expected << "*ESR?" << command << "*ESR?";
    require(transport.writes==expected, "semantic API changed command order, channel or numeric precision");
    transport.writes.clear();
    for(int channel : {0,5}) {
        require(!scope.setChannelTriggerLevel(channel,1,error) && !error.isEmpty(), "invalid level channel accepted");
        require(!scope.setVerticalScale(channel,1,error), "invalid scale channel accepted");
        require(std::isnan(scope.readMeasurement(channel,ScopeMeasurement::Rms)), "invalid measurement channel accepted");
    }
    for(double value : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        require(!scope.setVerticalScale(2,value,error), "invalid scale accepted");
    require(!scope.setChannelTriggerLevel(2,NAN,error) && transport.writes.isEmpty(), "invalid input reached transport");
    transport.failWrite="TRIGger:A:EDGE:SLOpe FALL";
    require(!scope.setEdgeSlope(ScopeTriggerSlope::Fall,error) && !error.isEmpty(), "failed write hidden");
    transport.failWrite.clear();
    transport.queued["*ESR?"]={"0","32"};
    require(!scope.setEdgeSlope(ScopeTriggerSlope::Fall,error) && error.contains("rejected"), "device rejection hidden");
    require(scope.setEdgeSlope(ScopeTriggerSlope::Fall,error) && error.isEmpty(), "retry failed");
    for(QByteArray response : {QByteArray("0"),QByteArray(":ACQUIRE:STATE STOP"),QByteArray("OFF")}) {
        transport.responses["ACQuire:STATE?"]=response;
        require(scope.acquisitionState()==ScopeAcquisitionState::Completed, "completed state decoding failed");
    }
    for(QByteArray response : {QByteArray("1"),QByteArray("RUN"),QByteArray("ON")}) {
        transport.responses["ACQuire:STATE?"]=response;
        require(scope.acquisitionState()==ScopeAcquisitionState::Running, "running state decoding failed");
    }
    for(QByteArray response : {QByteArray(""),QByteArray("invalid"),QByteArray("2")}) {
        transport.responses["ACQuire:STATE?"]=response;
        require(scope.acquisitionState()==ScopeAcquisitionState::Invalid && !scope.lastError().isEmpty(), "invalid state accepted");
    }
    const QList<QPair<QByteArray,ScopeTriggerState>> states{
        {"ARMED",ScopeTriggerState::Arming},{":TRIGGER:STATE READY",ScopeTriggerState::Ready},
        {"TRIGGER",ScopeTriggerState::Triggered},{"AUTO",ScopeTriggerState::Automatic},
        {"SAVE",ScopeTriggerState::Saving},{"SCAN",ScopeTriggerState::Invalid},{"bad",ScopeTriggerState::Invalid}};
    for(const auto &state : states) {
        transport.responses["TRIGger:STATE?"]=state.first;
        require(scope.triggerState()==state.second, "trigger state mapping changed");
    }
    transport.failWrite="ACQuire:STATE?";
    require(scope.acquisitionState()==ScopeAcquisitionState::Invalid, "failed status query accepted");
    transport.failWrite.clear();
    for(QByteArray response : {QByteArray("CH2"),QByteArray(":TRIGGER:A:EDGE:SOURCE CH2")}) {
        transport.source=response;require(scope.triggerChannel()==2, "CH2 source not decoded");
    }
    for(QByteArray response : {QByteArray("AUX"),QByteArray("MATH1"),QByteArray("CH0"),QByteArray("CH9"),QByteArray("bad")}) {
        transport.source=response;require(scope.triggerChannel()==0, "invalid trigger source guessed CH1");
    }
    transport.responses["MEASUrement:IMMed:VALue?"]="1.5";
    const QList<QPair<ScopeMeasurement,QString>> metrics{{ScopeMeasurement::Maximum,"MAXIMUM"},
        {ScopeMeasurement::Minimum,"MINIMUM"},{ScopeMeasurement::Rms,"RMS"},{ScopeMeasurement::Mean,"MEAN"}};
    for(const auto &metric : metrics) {
        transport.writes.clear();
        require(scope.readMeasurement(2,metric.first)==1.5,"typed metric read failed");
        expected={"*ESR?","MEASUrement:IMMed:SOUrce1 CH2","*ESR?","*ESR?",
                  "MEASUrement:IMMed:TYPe "+metric.second,"*ESR?","MEASUrement:IMMed:VALue?"};
        require(transport.writes==expected,"measurement protocol changed");
    }
}

void referenceDriverContract()
{
    ScriptedTransport transport;
    std::unique_ptr<Oscilloscope> scope(OscilloscopeFactory::createOscilloscope("MSO44B", &transport));
    scope->connect();
    transport.responses["HORizontal:SCAle?"] = ".01";
    transport.responses["HORizontal:DIVisions?"] = "10";
    transport.responses["HORizontal:FASTframe:STATE?"] = "0";
    transport.responses["ACQuire:MODe?"] = "SAMPLE";
    transport.responses["TRIGger:A:MODe?"] = "AUTO";
    transport.responses["ACQuire:STOPAfter?"] = "RUNSTOP";
    transport.responses["ACQuire:STATE?"] = "1";
    transport.responses["TRIGger:STATE?"] = "AUTO";
    transport.responses["ACQuire:NUMACq?"] = "0";
    QString error, state; int budget = 0;
    require(scope->beginAutoReference(budget,error), qPrintable(error));
    require(budget == 10000 && transport.writes.contains("ACQuire:STOPAfter RUNSTop") &&
            !transport.writes.contains("ACQuire:STOPAfter SEQuence"), "DC reference still depends on Single");
    transport.writes.clear();
    require(scope->pollAutoReference(state,error) == ScopeReferenceStatus::Pending &&
            !transport.writes.contains("ACQuire:STATE STOP"), "empty acquisition became a reference");
    transport.responses["ACQuire:NUMACq?"] = "1";
    transport.queued["ACQuire:STATE?"] = {"1","0"};
    require(scope->pollAutoReference(state,error) == ScopeReferenceStatus::Completed &&
            transport.writes.contains("ACQuire:STATE STOP"), "fresh AUTO record was not frozen");
    require(state.contains("count=1") && state.contains("AUTO"), "reference diagnostics lost actual state");
    transport.responses["TRIGger:A:MODe?"] = "NORMAL";
    require(!scope->beginAutoReference(budget,error) && error.contains("AUTO"), "wrong actual trigger mode accepted");
    transport.responses["TRIGger:A:MODe?"] = "AUTO";
    transport.responses["HORizontal:SCAle?"] = "3";
    transport.writes.clear();
    require(!scope->beginAutoReference(budget,error) && !transport.writes.contains("ACQuire:STATE RUN"),
            "excessive reference window started without a bounded budget");

    auto start = [&] {
        transport.queued.clear(); transport.failWrite.clear(); transport.writes.clear();
        transport.responses["HORizontal:SCAle?"] = ".01";
        transport.responses["ACQuire:MODe?"] = "SAMPLE";
        transport.responses["TRIGger:A:MODe?"] = "AUTO";
        transport.responses["ACQuire:STOPAfter?"] = "RUNSTOP";
        transport.responses["ACQuire:STATE?"] = "1";
        transport.responses["TRIGger:STATE?"] = "AUTO";
        transport.responses["ACQuire:NUMACq?"] = "0";
        require(scope->beginAutoReference(budget,error), "reference restart failed");
    };
    for (const QByteArray bad : {QByteArray("-1"),QByteArray("1.5"),QByteArray("bad")}) {
        start(); transport.responses["ACQuire:NUMACq?"] = bad;
        require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && error.contains("count"),
                "invalid acquisition count accepted");
    }
    start(); transport.responses["ACQuire:STATE?"] = "0";
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && error.contains("without a new"),
            "manual STOP with zero records became a reference");
    for (const QString key : {QString("TRIGger:A:MODe?"),QString("ACQuire:STOPAfter?")}) {
        start(); transport.responses[key] = key.contains("STOPAfter") ? "SEQUENCE" : "NORMAL";
        require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && error.contains("mismatch"),
                "changed front-panel mode accepted while waiting");
    }
    start(); transport.failWrite = "ACQuire:NUMACq?";
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && !error.isEmpty(),
            "counter transport error was hidden");
    start(); transport.responses["ACQuire:NUMACq?"] = "1"; transport.failWrite = "ACQuire:STATE STOP";
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && !error.isEmpty(),
            "failed freeze exposed a running record");
    start(); transport.queued["ACQuire:NUMACq?"] = {"2","1"};
    transport.queued["ACQuire:STATE?"] = {"1","0"};
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Error && error.contains("counter reset"),
            "counter rollback during STOP accepted");
    start(); transport.responses["ACQuire:MODe?"] = "AVERAGE";
    transport.responses["ACQuire:NUMAVg?"] = "4";
    require(scope->beginAutoReference(budget,error), "averaged reference setup failed");
    transport.responses["ACQuire:NUMACq?"] = "3"; transport.writes.clear();
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Pending &&
            !transport.writes.contains("ACQuire:STATE STOP"), "incomplete averaging was frozen");
    transport.responses["ACQuire:NUMACq?"] = "4"; transport.queued["ACQuire:STATE?"] = {"1","0"};
    require(scope->pollAutoReference(state,error)==ScopeReferenceStatus::Completed,
            "complete averaging was not frozen");
}

void periodDriverContract()
{
    ScriptedTransport transport;
    transport.responses = {{"HORizontal:DIVisions?", "10"}, {"ACQuire:STATE?", "1"},
        {"ACQuire:STOPAfter?", "RUNSTOP"}, {"TRIGger:A:MODe?", "NORMAL"}, {"MEASUrement:LIST?", ":MEASUREMENT:LIST MEAS1,MEAS3"},
        {"HORizontal:SCAle?", "0.02"}, {"MEASUrement:MEAS2:RESUlts:CURRentacq:MEAN?", "0.02"}};
    std::unique_ptr<Oscilloscope> scope(OscilloscopeFactory::createOscilloscope("MSO44B", &transport));
    scope->connect();
    QString error; QAtomicInt stop{0};
    auto session = scope->beginPeriodSession([&] { return !stop.loadAcquire(); }, error);
    require(bool(session) && session->horizontalDivisions()==10, "real PERIOD session setup failed");
    double scale=0, period=0;
    require(session->readTimeScale(scale,error) && scale==.02, "timebase query changed");
    require(session->prepareRecord(error) && session->readPeriod(period,error) && period==.02,
            "PERIOD record commands failed");
    require(session->setTimeScale(.01,error) && session->restore(true,error), "PERIOD restore failed");
    QStringList expected{"TRIGger:A:EDGE:SOUrce?", "DISplay:GLObal:CH2:STATE?", "HORizontal:DIVisions?",
        "ACQuire:STATE?", "ACQuire:STOPAfter?", "TRIGger:A:MODe?", "MEASUrement:LIST?", "HORizontal:SCAle?"};
    auto checked=[&](const QString &command){ expected << "*ESR?" << command << "*ESR?"; };
    checked("ACQuire:STATE STOP"); checked("MEASUrement:ADDNew \"MEAS2\"");
    checked("MEASUrement:MEAS2:TYPe PERIOD"); checked("MEASUrement:MEAS2:SOUrce1 CH2");
    expected << "MEASUrement:MEAS2:RESUlts:CURRentacq:MEAN?";
    checked("HORizontal:SCAle 0.01"); checked("ACQuire:STATE STOP");
    checked("MEASUrement:DELete \"MEAS2\""); checked("ACQuire:STOPAfter RUNSTOP"); checked("TRIGger:A:MODe NORMAL"); checked("ACQuire:STATE RUN");
    require(transport.writes==expected, "real PERIOD SCPI sequence changed");
    transport.responses["HORizontal:MODe?"]="MANUAL";
    transport.responses["HORizontal:MODe:MANual:CONFigure?"]="RECORDLENGTH";
    transport.responses["HORizontal:MODe:SAMPLERate?"]="12500000";
    transport.responses["HORizontal:MODe:RECOrdlength?"]="1000";
    IScopePeriodSession::HorizontalContext context;
    require(session->readHorizontalContext(context,error) && context.mode=="MANUAL" &&
            context.manualInputs=="RECORDLENGTH" && context.sampleRate==12500000 && context.recordLength==1000,
            "horizontal context readback failed");
    transport.responses["HORizontal:MODe:MANual:CONFigure?"]="invalid";
    require(!session->readHorizontalContext(context,error) && error.contains("Manual Inputs") &&
            context.manualInputs=="RECORDLENGTH", "invalid context published partial result");
    transport.responses["HORizontal:MODe?"]="AUTO";
    require(session->readHorizontalContext(context,error) && context.manualInputs.isEmpty(),
            "AUTO queried inapplicable Manual Inputs");
    transport.responses["HORizontal:MODe:RECOrdlength?"]="1000.5";
    require(!session->readHorizontalContext(context,error),"fractional record length accepted");
    transport.responses["HORizontal:MODe:RECOrdlength?"]="1000";
    transport.responses["HORizontal:MODe:SAMPLERate?"]="NA";
    require(!session->readHorizontalContext(context,error),"malformed sample rate accepted");
    session.reset();
    for(int failure=0;failure<3;++failure) {
        transport.writes.clear(); transport.failWrite.clear(); stop.storeRelease(0);
        session=scope->beginPeriodSession([&] { return !stop.loadAcquire(); },error);
        require(bool(session),"second session setup failed");
        if(failure==0) {
            transport.cancelAfter="MEASUrement:ADDNew \"MEAS2\""; transport.cancellation=&stop;
            require(!session->prepareRecord(error),"cancelled record setup continued");
            transport.cancelAfter.clear();
        } else require(session->prepareRecord(error),"record setup failed");
        if(failure==1) transport.failWrite="MEASUrement:DELete \"MEAS2\"";
        if(failure==2) session.reset();
        else require(session->restore(false,error)==(failure==0),"cleanup result incorrect");
        require(transport.writes.contains("ACQuire:STOPAfter RUNSTOP") &&
            !transport.writes.contains("ACQuire:STATE RUN"),"failed/cancelled session leaked state or restarted");
        session.reset();
    }
}

class Scope : public Oscilloscope {
public:
    QString model() const override { return "MSO44B"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString &, const QString &) override { return {}; }
    bool isConnected() const override { return connected; }
    void disconnect() override { connected = false; ++disconnects; }
    bool isRunning() override { m_lastError = pollError; return false; }
    void setTriggerLevel(double level) override
    {
        if (entered) entered->release();
        if (release) release->acquire();
        lastLevel = level;
    }
    QString pollError;
    bool connected = true;
    std::atomic<int> disconnects{0};
    double lastLevel = -1;
    QSemaphore *entered = nullptr, *release = nullptr;
};

void manualContract()
{
    auto scope = std::make_shared<Scope>();
    auto session = std::make_shared<CaptureSession>();
    ManualScopeControl control;
    control.setLeaseFactory([scope, session]() -> std::shared_ptr<void> { return CaptureLease::tryAcquire(session); });
    control.bind(scope.get());
    QSemaphore entered, release;
    scope->entered = &entered;
    scope->release = &release;
    control.setLevel(1.25);
    const bool started = entered.tryAcquire(1, 2000);
    if (!started) release.release();
    require(started && control.hasActiveCommand(), "headless command did not start");
    control.setLevel(3.5);
    require(session->isBusy(), "command lost its lease");
    control.bind(nullptr);
    release.release();
    until([&] { return !control.isBusy(); });
    require(scope->lastLevel == 1.25 && !session->isBusy() && !control.hasActiveCommand(),
            "command overlap or leaked busy state");
    control.bind(scope.get());
    int observed = 0;
    QObject::connect(&control, &ManualScopeControl::runningObserved, [&] { ++observed; });
    scope->pollError = "connection lost";
    control.poll();
    until([&] { return !control.isBusy(); });
    require(!control.isConnected() && scope->disconnects == 1 && observed == 0,
            "failed poll published a valid stopped state");
    control.setSuspended(true);
}

InstrumentSetupConfig configuration(QString model = "MSO44B")
{
    InstrumentSetupConfig config;
    InstrumentConfig instrument;
    instrument.type = "Oscilloscope";
    instrument.modelName = model;
    config.instruments.append(instrument);
    return config;
}
void leaseContract()
{
    auto first = std::make_shared<Scope>(), second = std::make_shared<Scope>();
    int connections = 0;
    auto coordinator = std::make_unique<ScopeSessionCoordinator>([&](const InstrumentSetupConfig &) {
        return OscilloscopeManager::OscMap{{"MSO44B", ++connections == 1 ? first : second}};
    });
    coordinator->queueConfiguration(configuration());
    until([&] { return coordinator->current() && !coordinator->isConfigurationBusy(); });
    auto borrowed = coordinator->borrowForTest();
    require(borrowed.get() == first.get(), "scope borrow failed");
    require(!coordinator->pollingLeaseFactory()(), "poll raced test lease");
    coordinator->queueConfiguration(configuration());
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 700) { QCoreApplication::processEvents(); QThread::msleep(1); }
    require(connections == 1 && first->disconnects == 0, "borrowed scope replaced early");
    borrowed.reset();
    until([&] { return coordinator->current() == second && !coordinator->isConfigurationBusy(); });
    require(first->disconnects == 1, "old scope not retired exactly once");
    auto polling = coordinator->pollingLeaseFactory()();
    require(bool(polling) && !coordinator->borrowForTest(), "borrow raced polling lease");
    coordinator.reset();
    require(second->disconnects == 0, "active poll scope disconnected on owner destruction");
    polling.reset();
    until([&] { return second->disconnects == 1; });
}

void ruleContract()
{
    for (const auto &rule : {TaskSettingRules::settle, TaskSettingRules::triggerTimeout,
                            TaskSettingRules::phaseTimeout, TaskSettingRules::discharge, TaskSettingRules::trials}) {
        for (double value : {double(rule.minimum), double(rule.maximum), double(rule.minimum - 1),
                             double(rule.maximum + 1), 1.5}) {
            const bool accepted = rule.accepts(value);
            QString error;
            require(TestSequenceSettingsValidation::config("turnOn", {{rule.key, value}}, error) == accepted,
                    "XML and common numeric bounds differ");
            bool runtimeAccepted = true;
            try { TaskSettings::bounded({{rule.key, value}}, rule); }
            catch (const std::invalid_argument &) { runtimeAccepted = false; }
            require(runtimeAccepted == accepted, "runtime and common numeric bounds differ");
        }
        require(TaskSettings::bounded({}, rule) == rule.initial, "runtime default differs");
        QString error;
        require(!TestSequenceSettingsValidation::config("turnOn", {{rule.key, "5"}}, error), "XML accepted string as numeric");
        bool rejected = false;
        try { TaskSettings::bounded({{rule.key, "5"}}, rule); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "runtime accepted string as numeric");
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QString scenario = argc > 1 ? argv[1] : "";
        if (scenario == "driver") driverContract();
        else if (scenario == "measurement") { measurementDriverContract(); referenceDriverContract(); }
        else if (scenario == "period") periodDriverContract();
        else if (scenario == "manual") manualContract();
        else if (scenario == "lease") leaseContract();
        else if (scenario == "rules") ruleContract();
        else throw std::runtime_error("unknown scenario");
        require(QThreadPool::globalInstance()->waitForDone(4000), "worker did not finish");
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
