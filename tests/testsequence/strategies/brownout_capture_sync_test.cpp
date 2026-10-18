#define main original_brownout_main
#include "brownout_test.cpp"
#undef main

struct SyncState : State {
    std::atomic_bool reading{false}, saving{false}, rearming{false}, writing{false};
    std::atomic_int monitored{0};
    int saved=0, readyChecks=0;
};
class SyncIo : public Io {
public:
    explicit SyncIo(SyncState &state) : Io(state), state(state) {}
    void setVoltage(double voltage) override {
        expect(!state.reading && !state.saving && !state.rearming,"voltage write overlapped a scope transfer/rearm");
        state.writing=true;
        if(state.scenario=="slow_write")QThread::msleep(130);
        Io::setVoltage(voltage);
        state.writing=false;
    }
    double outputVoltage() override {
        ++state.monitored;
        if(state.scenario=="monitor_failure" && state.saving)throw std::runtime_error("monitor failed during capture");
        return Io::outputVoltage();
    }
    SyncState &state;
};
class SyncSession : public Session {
public:
    SyncSession(SyncState &state,BrownOut::Settings cfg) : Session(state,cfg),state(state) {}
    BrownOutRecordStatus poll(BrownOutRecord &record,QString &error) override {
        if(armed && state.steps>0) {
            state.reading=true;
            expect(!state.writing,"poll started before voltage write finished");
            const int step=state.steps.load();
            QThread::msleep(150);
            expect(state.steps==step,"voltage changed during waveform read");
            state.reading=false;
        }
        const auto status=Session::poll(record,error);
        if(armed && status==BrownOutRecordStatus::Ready && state.scenario=="query_failure") {
            error="CURVe read failed";
            return BrownOutRecordStatus::Error;
        }
        return status;
    }
    bool readyToRamp(QString &error) override {
        ++state.readyChecks;
        if(state.records>0) {
            state.rearming=true;
            const int step=state.steps.load();
            QThread::msleep(state.scenario=="rearm_timeout"?1100:120);
            expect(state.steps==step&&!state.writing,"ramp resumed before READY");
            state.rearming=false;
            if(state.scenario=="rearm_failure") {error="READY query failed";return false;}
            if(state.scenario=="stop_rearm")state.stop->storeRelease(1);
        }
        return Session::readyToRamp(error);
    }
    SyncState &state;
};
class SyncScope : public Scope {
public:
    explicit SyncScope(SyncState &state) : Scope(state),state(state) {}
    std::unique_ptr<IScopeBrownOutSession> beginBrownOut(const BrownOut::Settings &cfg,QAtomicInt &,QString &) override {
        return std::make_unique<SyncSession>(state,cfg);
    }
    SyncState &state;
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        SyncState state;state.scenario=app.arguments().value(1);QAtomicInt stop{0};state.stop=&stop;
        SyncScope scope(state);auto values=settings();values["capture_enabled"]=true;values["mode"]="TRACK";
        BrownOut::Settings cfg(values);cfg.timeoutMs=350;
        auto capture=[&](const ScopeCaptureRecord &record) {
            ++state.saved;
            if(record.phase=="Auto reference")return;
            expect(!state.writing,"Capture started with a voltage write in flight");
            const int step=state.steps.load(),monitored=state.monitored.load();
            state.saving=true;
            QThread::msleep(550); // Longer than the active ramp watchdog, intentionally.
            expect(state.steps==step,"voltage continued descending while saving");
            expect(state.monitored>monitored,"Vout monitoring stopped during capture");
            if(state.scenario=="monitor_failure")expect(state.closed,"monitor fault did not shut down outputs during capture");
            state.saving=false;
            if(state.scenario=="save_failure")throw std::runtime_error("SAVE failed");
            if(state.scenario=="stop_capture")stop.storeRelease(1);
        };
        auto factory=makeMeasurementStrategyFactory({});
        const auto result=factory(BrownOutMeasurementRequest{cfg,scope,[&]{return std::make_unique<SyncIo>(state);},capture})->execute(stop);
        expect(state.cleaned==1&&state.finish==1&&state.closed,"failure/Stop skipped cleanup or deadlocked");
        const bool success=state.scenario=="success"||state.scenario=="slow_write";
        expect(result.success==success,qPrintable(result.error));
        expect(state.saved>=2,"completed event was not archived");
        if(success)expect(state.steps==3&&state.saved==state.records+1&&state.readyChecks>=3,"missing event or READY handshake");
        else expect(!result.error.isEmpty(),"failure silently passed");
        if(state.scenario=="query_failure")expect(result.error.contains("CURVe read failed")&&result.channels.isEmpty(),"diagnostic became valid measurement");
        std::cout<<"PASS Brown Out synchronized Capture "<<state.scenario.toStdString()<<'\n';
    } catch(const std::exception &e) {std::cerr<<e.what();return 1;}
}
