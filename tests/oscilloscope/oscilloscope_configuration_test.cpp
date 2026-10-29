#include "../support/measurement_scope_fixture.h"
#include "msoseries456.h"
#include "msoperiodsession.h"
#include "../support/configuration_test_helpers.h"
#include "oscilloscopeautoperiod.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <iostream>
#include <cmath>
#include <stdexcept>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Transport : public ICommunication {
public:
    QStringList writes;
    QList<QByteArray> replies;
    QString fail;
    bool consumed = false;
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return "simulated failure"; }
    int write(const QByteArray& data) override {
        consumed = false;
        writes << QString::fromUtf8(data);
        return writes.last() == fail ? -1 : data.size();
    }
    int read(QByteArray& data, int) override {
        if (consumed) { data.clear(); return 0; }
        consumed = true;
        if (replies.isEmpty()) return -1;
        data = replies.takeFirst();
        return data.size();
    }
};
class Scope : public MSOSeries456 {
public:
    using MSOSeries456::MSOSeries456;
    QString model() const override { return "MSOSeries456"; }
    QString vendor() const override { return "offline"; }
    int getTotalChannel() override { return 4; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    bool writeConfigurationCommand(const QString& command, QString& error) override {
        return writeScpiSettingChecked(command,error);
    }
};

class PeriodScope : public Scope {
public:
    std::unique_ptr<IScopePeriodSession> beginPeriodSession(IScopePeriodSession::Continue ready, QString &error) override {
        return makeMsoPeriodSession(*this, std::move(ready), error);
    }
    QStringList commands;
    double scale = .2;
    double period = .001;
    QList<double> capturePeriods;
    bool rejectTimeScale = false;
    double minimumTimeScale = 0;
    QString horizontalMode = "AUTO", manualInputs = "RECORDLENGTH", readFailure;
    QStringList queries;
    QString source = "CH1";
    QString state = "1";
    QString failure;
    QString throwingCommand;
    int captures = 0;
    int captureDelayMs = 0, measurementDelayMs = 0, referenceBudgetMs = 10000;
    QElapsedTimer captureTimer, resultTimer;
    bool malformed = false;
    double verticalScale = 1;
    int clippedCaptures = 0, periodReads = 0;
    bool stuckScale = false, throwClip = false;
    QAtomicInt* cancelClip = nullptr;
    bool isChannelEnabled(int ch) override { return ch <= 2; }
    bool isClipping(int ch) override {
        if (throwClip) throw std::runtime_error("clipping query failed");
        if (cancelClip) cancelClip->storeRelease(1);
        return ch == 2 && captures <= clippedCaptures;
    }
    double getChannelScale(int) override { return verticalScale; }
    double measureSignalPeak(int, const QString& type) override { return type=="MINimum"?-1:1; }
    QAtomicInt* cancel = nullptr;
    bool queryConfiguration(const QString& command, QString& response, QString& error) override {
        queries << command;
        if (command == readFailure) { error="injected horizontal query error"; return false; }
        if (command == "*ESR?") response = "0";
        else if (command == "HORizontal:MODe?") response = horizontalMode;
        else if (command == "HORizontal:MODe:MANual:CONFigure?") response = manualInputs;
        else if (command == "HORizontal:MODe:SAMPLERate?") response = "1000000";
        else if (command == "HORizontal:MODe:RECOrdlength?") response = QString::number(std::round(scale * 1e7));
        else if (command.startsWith("CH")) response = "0";
        else if (command == "TRIGger:A:EDGE:SOUrce?") response = source;
        else if (command == "TRIGger:A:MODe?") response = "NORMAL";
        else if (command.startsWith("DISplay")) response = "1";
        else if (command == "HORizontal:DIVisions?") response = "10";
        else if (command == "HORizontal:SCAle?") response = QString::number(scale, 'g', 16);
        else if (command == "ACQuire:STATE?") response = state;
        else if (command == "ACQuire:STOPAfter?") response = "RUNSTOP";
        else if (command == "MEASUrement:LIST?") response = ":MEASUREMENT:LIST MEAS1,MEAS3";
        else if (command.endsWith(":MEAN?")) {
            require(captures > clippedCaptures, "PERIOD queried on clipped record");
            ++periodReads;
            response = malformed ? "bad" : QString::number(resultTimer.elapsed() < measurementDelayMs ? 9.91e37 : (capturePeriods.isEmpty() ? period : capturePeriods[qMin(captures - 1, capturePeriods.size() - 1)]), 'g', 16);
            if (cancel) cancel->storeRelease(1);
        } else return false;
        return true;
    }
    bool writeConfigurationCommand(const QString& command, QString& error) override {
        commands << command;
        if (command == throwingCommand && captures > 0) throw std::runtime_error("cleanup threw");
        if (command == failure) { error = "simulated reject"; return false; }
        if (command.startsWith("HORizontal:SCAle ") && !rejectTimeScale)
            scale = std::max(minimumTimeScale, command.section(' ', -1).toDouble());
        if (command.startsWith("CH2:SCAle ") && !stuckScale) verticalScale = command.section(' ', -1).toDouble();
        return true;
    }
    bool beginSingleAcquisition() override {
        require(commands.contains("MEASUrement:MEAS2:SOUrce1 CH1"), "PERIOD not configured before capture");
        ++captures; captureTimer.start();resultTimer.invalidate();return true;
    }
    QString getTriggerState() override { return "AUTO"; }
    bool beginAutoReference(int &budget, QString &) override {
        budget = referenceBudgetMs;
        return beginSingleAcquisition(); // Preserve the fake's timing hooks; driver protocol is tested separately.
    }
    ScopeReferenceStatus pollAutoReference(QString &state, QString &) override {
        state = "fake AUTO reference";
        return getAcquisitionState() == "0" ? ScopeReferenceStatus::Completed : ScopeReferenceStatus::Pending;
    }
    QString getAcquisitionState() override {
        if (captures==1 && captureTimer.elapsed()<captureDelayMs) return "1";
        if (!resultTimer.isValid()) resultTimer.start();
        return "0";
    }
};
void testAcceptedPeriodWindow() {
    QString error; QAtomicInt stop(0);
    PeriodScope scope; scope.scale=8e-6; scope.period=11.89e-6; scope.rejectTimeScale=true;
    const bool accepted = adjustOscilloscopeAutoPeriod(&scope,stop,error);
    require(accepted, qPrintable(error));
    require(scope.captures==3 && scope.scale==8e-6,
            "stable 6.73-cycle initial window must be accepted after three fresh records");
    for (const auto &command : scope.commands)
        require(!command.startsWith("HORizontal:SCAle "), "accepted window changed time scale");
    std::cout << "PASS initial 6.73-cycle window without scale writes\n";
    for (double initial : {.0003, .0008}) {
        PeriodScope boundary; boundary.scale=initial; boundary.period=.001; boundary.rejectTimeScale=true;
        require(adjustOscilloscopeAutoPeriod(&boundary,stop,error) && boundary.captures==3,
                "inclusive 3/8-cycle boundary rejected");
    }
    for (const auto &cycles : {QVector<double>{8.2,8.1,8.,8.,8.},
                              QVector<double>{2.9,2.95,3.,3.,3.},
                              QVector<double>{8.,8.1,8.,8.,8.}}) {
        PeriodScope crossing; crossing.scale=.0008; crossing.rejectTimeScale=true;
        for(double count:cycles) crossing.capturePeriods.append(.008/count);
        QStringList crossingLogs;
        require(adjustOscilloscopeAutoPeriod(&crossing,stop,error,{}, {},
                    [&](const QString &line){crossingLogs<<line;}), qPrintable(error));
        require(crossing.captures==5,"Auto Period accepted before three consecutive in-window records");
        for(const auto &command:crossing.commands)
            require(!command.startsWith("HORizontal:SCAle "),"boundary crossing unnecessarily changed time scale");
        require(crossingLogs.join('\n').contains("in-window=3/3"),"consecutive window diagnostic missing");
    }
    PeriodScope quantized; quantized.scale=20e-6; quantized.period=11.89e-6; quantized.minimumTimeScale=8e-6;
    quantized.horizontalMode="MANUAL";
    QStringList logs;
    const bool quantizedOk=adjustOscilloscopeAutoPeriod(&quantized,stop,error,{}, {},
        [&](const QString &line){logs<<line;});
    require(quantizedOk, qPrintable(error));
    require(quantized.captures==6 && quantized.scale==8e-6,
            "quantized timebase was not verified with three new records");
    const auto output=logs.join('\n');
    require(output.contains("Mode=MANUAL") && output.contains("Inputs=Time Scale + Sample Rate") &&
            output.contains("RECORDLENGTH") && output.contains("target=") && output.contains("actual=") &&
            output.contains("attempt=6/24") && output.contains("adjustments=1/6") &&
            output.contains("SampleRate=") && output.contains("RecordLength=") && output.contains("accepted"),
            "Auto Period diagnostic context/counters missing");
    PeriodScope manual; manual.scale=.0005; manual.horizontalMode="MANUAL"; manual.manualInputs="HORIZONTALSCALE";
    logs.clear();
    require(adjustOscilloscopeAutoPeriod(&manual,stop,error,{}, {},[&](const QString &line){logs<<line;}) &&
            logs.join(' ').contains("Inputs=Sample Rate + Record Length"), "Manual Inputs mapping reversed");
    PeriodScope contextError; contextError.readFailure="HORizontal:MODe:RECOrdlength?";
    const bool contextOk=adjustOscilloscopeAutoPeriod(&contextError,stop,error);
    require(!contextOk && error.contains("injected horizontal query error") && contextError.captures==0 &&
            contextError.commands.contains("ACQuire:STATE STOP"), "horizontal context error lost or skipped cleanup");
    PeriodScope sixth;
    for(int group=0;group<7;++group) {
        const double period=.001/std::pow(2.0,std::min(group,5));
        for(int i=0;i<3;++i) sixth.capturePeriods.append(period);
    }
    const bool sixthOk=adjustOscilloscopeAutoPeriod(&sixth,stop,error);
    require(sixthOk, qPrintable(error));
    require(sixth.captures==21,"sixth adjustment never received its verification acquisitions");
    int writes=0; for(const auto &command:sixth.commands) writes+=command.startsWith("HORizontal:SCAle ");
    require(writes==6,"timebase adjustment budget changed");
    PeriodScope exhausted;
    for(int group=0;group<7;++group)
        for(int i=0;i<3;++i) exhausted.capturePeriods.append(.001/std::pow(2.0,group));
    const bool exhaustedOk=adjustOscilloscopeAutoPeriod(&exhausted,stop,error);
    writes=0; for(const auto &command:exhausted.commands) writes+=command.startsWith("HORizontal:SCAle ");
    require(!exhaustedOk && writes==6 && exhausted.captures==21 && error.contains("adjustments=6/6"),
            "exhausted adjustment budget wrote a seventh scale or lost diagnostics");
    PeriodScope unstable; unstable.scale=20e-6; unstable.minimumTimeScale=8e-6;
    unstable.capturePeriods={11.89e-6,11.89e-6,11.89e-6};
    for(int i=3;i<24;++i) unstable.capturePeriods.append(i%2 ? 11.89e-6 : 20e-6);
    require(!adjustOscilloscopeAutoPeriod(&unstable,stop,error) && unstable.captures==24 &&
            error.contains("attempt=24/24"), "quantized window accepted without fresh stable PERIOD");
    std::cout << "PASS window boundaries, quantized verification, diagnostics and sixth adjustment\n";
}
void testAutoPeriod() {
    QString error; QAtomicInt stop(0);
    PeriodScope scope;
    int archived=0;
    require(adjustOscilloscopeAutoPeriod(&scope, stop, error, [&](const ScopeCaptureRecord &record) {
        require(record.phase=="Auto Period" && !record.clipped && record.channels.size()==2,
                "Auto Period capture context missing"); ++archived;
    }), "auto period failed");
    require(archived==scope.captures,"Auto Period completed records omitted");
    require(scope.captures == 6 && std::abs(scope.scale - .0005) < 1e-12, "five cycle scale incorrect");
    require(scope.commands.contains("MEASUrement:ADDNew \"MEAS2\"")
        && !scope.commands.contains("MEASUrement:DELete \"MEAS1\""), "existing measurement overwritten");
    require(scope.commands.last() != "ACQuire:STATE RUN", "Capture resumed unobserved continuous RUN");
    PeriodScope averaged; averaged.scale=.1; averaged.period=.2; averaged.referenceBudgetMs=37000;
    QStringList referenceLog;
    require(adjustOscilloscopeAutoPeriod(&averaged,stop,error,{}, {},
                [&](const QString &line){referenceLog<<line;}) &&
            referenceLog.join(' ').contains("wait limit=37000"),
            "Auto Period truncated the averaging-aware reference budget to one record window");
    // 20 ms/div must shrink for ten cycles, but grow when only one cycle fits.
    for (double actualPeriod : {0.02, 0.2}) {
        PeriodScope range; range.scale = 0.02; range.period = actualPeriod;
        require(adjustOscilloscopeAutoPeriod(&range, stop, error), qPrintable(error));
        require(std::abs(range.scale - actualPeriod * 5 / 10) < 1e-12,
                "Auto Period failed bidirectional timescale adjustment");
    }
    PeriodScope transientReading; transientReading.scale = .02;
    transientReading.capturePeriods = {.00002, .02, .02, .02, .02, .02, .02};
    require(adjustOscilloscopeAutoPeriod(&transientReading, stop, error), qPrintable(error));
    require(std::abs(transientReading.scale - .01) < 1e-12,
            "first short PERIOD caused incorrect time scale");
    require(!transientReading.commands.contains("HORizontal:SCAle 1e-05"),
            "unstable first capture changed timescale");
    PeriodScope unstable;
    for (int i=0;i<24;++i) unstable.capturePeriods.append(i%2 ? .001 : .02);
    require(!adjustOscilloscopeAutoPeriod(&unstable, stop, error) && unstable.captures == 24,
            "inconsistent fresh acquisitions accepted or unbounded");
    PeriodScope rejected; rejected.rejectTimeScale = true;
    require(!adjustOscilloscopeAutoPeriod(&rejected, stop, error) && rejected.captures == 3
            && error.contains("not applied"), "unapplied time scale used for verification");
    PeriodScope clipped; clipped.clippedCaptures = 7;
    int archivedClipped=0, allRecords=0;
    require(adjustOscilloscopeAutoPeriod(&clipped, stop, error, [&](const ScopeCaptureRecord &record) {
        ++allRecords; archivedClipped+=record.clipped;
    })
        && clipped.captures == 13 && std::abs(clipped.verticalScale - 128) < 1e-6 && clipped.periodReads >= 6,
        "vertical retries consumed period attempts or failed to reacquire");
    require(allRecords==13 && archivedClipped==7,"Auto Period clipping diagnostics missing");
    for (int failure = 0; failure < 4; ++failure) {
        PeriodScope failed; failed.clippedCaptures = 100;
        if (failure == 0) failed.stuckScale = true;
        if (failure == 1) failed.failure = "CH2:SCAle 2";
        if (failure == 2) failed.throwClip = true;
        require(!adjustOscilloscopeAutoPeriod(&failed, stop, error) && !error.isEmpty()
            && failed.periodReads == 0 && failed.captures <= 9,
            "clipping failure ignored or unbounded");
        require(failed.commands.contains("ACQuire:STOPAfter RUNSTOP")
            && !failed.commands.contains("ACQuire:STATE RUN"), "clipping failure cleanup missing");
    }
    PeriodScope diagnostic;diagnostic.throwClip=true;int diagnosticRecords=0;
    require(!adjustOscilloscopeAutoPeriod(&diagnostic,stop,error,[&](const ScopeCaptureRecord &record){
        require(record.channels.size()==2,"Auto Period diagnostic lost channels");++diagnosticRecords;
    })&&diagnosticRecords==1&&error.contains("clipping query failed")&&diagnostic.periodReads==0,
        "Auto Period query failure lost its frozen record or falsely passed");
    PeriodScope clipCancelled; clipCancelled.cancelClip = &stop;
    require(!adjustOscilloscopeAutoPeriod(&clipCancelled, stop, error)
        && clipCancelled.periodReads == 0 && error.contains("Stopped", Qt::CaseInsensitive),
        "clipping cancellation ignored");
    stop.storeRelease(0);
    PeriodScope absent; absent.period = 9.91e37;
    require(!adjustOscilloscopeAutoPeriod(&absent, stop, error) && absent.captures > 0 && absent.captures <= 6,
        "invalid sentinel accepted or retry unbounded");
    PeriodScope slow; slow.scale=1;slow.period=2;slow.captureDelayMs=5200;slow.measurementDelayMs=1200;
    require(adjustOscilloscopeAutoPeriod(&slow, stop, error), qPrintable(error));
    require(std::abs(slow.scale-1)<1e-12,"slow signal scale incorrect");
    PeriodScope excessive; excessive.scale=100;
    require(!adjustOscilloscopeAutoPeriod(&excessive,stop,error) && excessive.captures==0,
            "excessive window started capture");
    PeriodScope stopped; stopped.state = "0";
    require(adjustOscilloscopeAutoPeriod(&stopped, stop, error)
        && !stopped.commands.contains("ACQuire:STATE RUN"), "stopped scope restarted");
    PeriodScope invalid; invalid.malformed = true;
    require(!adjustOscilloscopeAutoPeriod(&invalid, stop, error), "malformed period accepted");
    require(invalid.commands.contains("MEASUrement:DELete \"MEAS2\"")
        && !invalid.commands.contains("ACQuire:STATE RUN"), "failure cleanup incorrect");
    PeriodScope cleanup; cleanup.failure = "ACQuire:STOPAfter RUNSTOP";
    require(!adjustOscilloscopeAutoPeriod(&cleanup, stop, error), "cleanup error ignored");
    PeriodScope throwingCleanup; throwingCleanup.throwingCommand = "ACQuire:STATE STOP";
    require(!adjustOscilloscopeAutoPeriod(&throwingCleanup, stop, error)
        && error.contains("cleanup threw")

        && throwingCleanup.commands.contains("ACQuire:STOPAfter RUNSTOP"),
        "cleanup exception escaped or skipped remaining cleanup");
    PeriodScope aux; aux.source = "AUX";
    require(!adjustOscilloscopeAutoPeriod(&aux, stop, error) && aux.commands.isEmpty(), "AUX accepted");
    PeriodScope cancelled; cancelled.cancel = &stop;
    require(!adjustOscilloscopeAutoPeriod(&cancelled, stop, error), "cancellation ignored");
    require(cancelled.commands.contains("MEASUrement:DELete \"MEAS2\""), "cancel leaked badge");
    stop.storeRelease(0);
    PeriodScope deferred;
    require(applyOscilloscopeConfiguration(&deferred,
        {{"timescale", "20ms"}}, stop, error)
        && deferred.captures == 0 && std::abs(deferred.scale - .02) < 1e-12,
        "Write must set initial timescale without acquiring a waveform");
    PeriodScope ignored;
    require(applyOscilloscopeConfiguration(&ignored, {{"ignore_horizontal", true}}, stop, error)
        && ignored.captures == 0, "Ignore did not bypass Auto Period");
    QStringList plan;
    require(!buildOscilloscopeCommands({{"horz_mode","MANual"},{"sample_rate","bad"}},
        "MSOSeries456",4,plan,error), "Invalid manual rate accepted");
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        testAcceptedPeriodWindow();
        if (app.arguments().contains("--window")) return 0;
        testAutoPeriod();
        QVariantMap cfg{{"horz_mode","MANual"},{"horz_config","RECORDLength"},
            {"timescale","200ms"},{"sample_rate","6.25GS/s"},{"record_length","62.5M"},
            {"fastacq",true},{"acq_mode","SAMple"},{"trig_source","AUX"},
            {"ch1_enabled",true},{"ch1_termination","1MΩ"},{"ch1_bandwidth","200MHz"},
            {"ch1_position",-2},{"ch1_scale","100mV"},{"ch1_coupling","DC"}};
        const auto typed = decodeOscilloscopeSettings(cfg,"MSOSeries456",4);
        require(typed.horizontal.secondsPerDivision == std::optional<double>(0.2) && typed.channels.size()==1
            && typed.channels[0].scale == std::optional<double>(0.1) && typed.channels[0].positionDivisions == std::optional<double>(-2), "typed units/position wrong");
        const auto typedCommands = compileMsoConfiguration(typed);
        QStringList commands; QString error;
        require(buildOscilloscopeCommands(cfg,"MSOSeries456",4,commands,error), "manual config rejected");
        require(commands==typedCommands,"map and typed compilation disagree");
        for (const auto& required : QStringList{"HORizontal:MODe:SAMPLERate 6250000000",
            "HORizontal:SCAle 0.2","CH1:TERmination 1000000",
            "CH1:BANdwidth 200000000","CH1:POSition -2","ACQuire:FASTAcq:STATE ON"})
            require(commands.contains(required), "missing setting or wrong unit conversion");
        cfg["horz_mode"]="AUTO"; cfg["sample_rate"]="bad";
        require(buildOscilloscopeCommands(cfg,"MSOSeries456",4,commands,error), "AUTO parsed manual fields");
        require(commands.filter("SAMPLERate").isEmpty(), "AUTO wrote manual fields");
        cfg["ch1_scale"]="garbage";
        QVariantMap currentProbe{{"ch1_enabled",true},{"ch1_scale","2mA"}};
        require(buildOscilloscopeCommands(currentProbe,"MSOSeries456",4,commands,error)
            && commands.contains("CH1:SCAle 0.002"), "current scale unit conversion failed");
        require(!buildOscilloscopeCommands(cfg,"MSOSeries456",4,commands,error) && commands.isEmpty(), "bad numeric value accepted");
        cfg["ignore_channels"]=true;
        require(buildOscilloscopeCommands(cfg,"MSOSeries456",4,commands,error), "ignored fields parsed");
        QVariantMap dpo{{"ch1_enabled",true},{"ch1_bandwidth","Full"},{"ch1_scale","1V"}};
        for (const auto& model : {"DPO4000","DPO7000"}) {
            require(!buildOscilloscopeCommands(dpo,model,4,commands,error) && commands.isEmpty(), "Removed DPO accepted");
        }
        Transport transport; Scope scope(&transport); QAtomicInt stop(0);
        QVariantMap one{{"timescale","1ms"}};
        transport.replies={"0","0","0","AUTO","0.001","1000000","10000","10","0"};
        require(applyOscilloscopeConfiguration(&scope,one,stop,error), "valid write failed");
        transport.replies={"0","32"};
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error) && error.contains("rejected"), "instrument error accepted");
        transport.replies={"0"}; transport.fail="HORizontal:SCAle 0.001";
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error) && error.contains("Write failed"), "write failure accepted");
        transport.fail.clear(); transport.replies={"0","bad"};
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error), "invalid status accepted");
        transport.replies={"0","0.5"};
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error), "fractional status accepted");
        transport.replies={"16"};
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error), "existing error ignored");
        stop.storeRelease(1); const auto count=transport.writes.size();
        require(!applyOscilloscopeConfiguration(&scope,one,stop,error) && transport.writes.size()==count, "cancelled write executed");
        std::cout << "PASS configuration mapping and checked writes\n";
    } catch(const std::exception& error) { std::cerr << error.what(); return 1; }
}
