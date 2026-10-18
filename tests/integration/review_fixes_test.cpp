#include "msoseries456.h"
#include <atomic>
#include "chroma61505.h"
#include "chroma63600.h"
#include "relay_rtu_4.h"
#include "instrumentexecutor.h"
#include "resourcecleaner.h"
#include "msoseries456triggercontroller.h"
#include "msoseries456triggerwidget.h"
#include "triggermodelcatalog.h"
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QSemaphore>
#include "smartstepspinbox.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#include "../support/trigger_step_cases.h"
template<class F> void mustFail(F f) { bool failed = false; try { f(); } catch (...) { failed = true; } require(failed, "failure was swallowed"); }
struct RelayFixture { QVector<RelayBase*> relays; QMap<QString,ICommunication*> commMap; QVector<InstrumentConfig> configurations; bool success=false; QString errorMessage; };
static RelayFixture relayResult;
struct ACFixture { ACSource* source=nullptr; ICommunication* comm=nullptr; bool success=false; QString errorMessage; };
static ACFixture acResult;
struct DCFixture { QVector<DCLoad*> dcLoads; QMap<QString,ICommunication*> commMap; bool success=false; QString errorMessage; };
static DCFixture dcResult;
InstrumentCreator::ACSourceResult InstrumentCreator::createACSource(const InstrumentSetupConfig&) { return {acResult.source, acResult.comm, acResult.success, acResult.errorMessage}; }
void ResourceCleaner::cleanupACSource(ACSource*, ICommunication*) {}
InstrumentCreator::DCLoadResult InstrumentCreator::createDCLoads(const InstrumentSetupConfig&, TableKind) { DCLoadResult r; r.m_dcLoads=dcResult.dcLoads; r.m_commMap=dcResult.commMap; r.success=dcResult.success; r.errorMessage=dcResult.errorMessage; return r; }
InstrumentCreator::DCLoadResult InstrumentCreator::createDCLoadsForShutdown(const InstrumentSetupConfig& c, TableKind k) { return createDCLoads(c,k); }
InstrumentCreator::RelayResult InstrumentCreator::createRelays(const InstrumentSetupConfig&) { RelayResult r; r.m_relays=relayResult.relays; r.m_commMap=relayResult.commMap; r.m_configurations=relayResult.configurations; r.success=relayResult.success; r.errorMessage=relayResult.errorMessage; return r; }
void ResourceCleaner::cleanupDCLoads(QVector<DCLoad*>&, QMap<QString, ICommunication*>&) {}
void ResourceCleaner::cleanupRelays(QVector<RelayBase*>&, QMap<QString, ICommunication*>&) {}

class Comm : public ICommunication {
public:
    QList<QByteArray> writes;
    QByteArray reply, failOn;
    int writeLimit = -1, readChunk = 8;
    bool opened = true;
    bool open() override { return opened = true; }
    void close() override { opened = false; }
    bool isOpen() const override { return opened; }
    QString lastError() const override { return "injected failure"; }
    int write(const QByteArray& data) override {
        writes << data;
        if (!failOn.isEmpty() && data.toUpper().contains(failOn)) return -1;
        return writeLimit < 0 ? data.size() : qMin(writeLimit, int(data.size()));
    }
    int read(QByteArray& data, int maxLen) override {
        if (reply.isEmpty()) return -1;
        data = reply.left(qMin(maxLen, readChunk)); reply.remove(0, data.size()); return data.size();
    }
};
class Load : public DCLoad {
public:
    bool failOff = false, failSelect = false; int off = 0, on = 0, programmed = 0;
    DynamicCurrentParam parameter;
    QString model() const override { return "fake"; }
    QString vendor() const override { return "fake"; }
    bool outputEnabled() override { return on > off; }
    void setChannel(int) override { if (failSelect) throw std::runtime_error("select"); }
    void setLoadOn() override { ++on; }
    void setLoadOff() override { ++off; if (failOff) throw std::runtime_error("off"); }
    void setStaticCurrent(const StaticCurrentParam&) override { ++programmed; }
    void setDynamicCurrent(const DynamicCurrentParam& p) override { ++programmed; parameter = p; }
    int getNumSegments() const override { return 2; }
};
class Relay : public RelayBase {
public:
    Relay(){setMaxChannels(4);}
    QVector<int> off;
    bool isConnected() const override { return true; }
    bool turnOn(int) override { throw std::runtime_error("unexpected ON"); }
    bool turnOff(int ch) override { off << ch; return true; }
    bool setMultiChannels(const QVector<int>&, bool) override { return false; }
    RelayStatus getStatus(int) override { return RelayStatus::Open; }
    QString model() const override { return "fake"; }
    QString vendor() const override { return "fake"; }
};
class Scope : public MSOSeries456 {
public:
    int polls = 0, disconnects = 0;
    bool isConnected() const override { return true; }
    bool isRunning() override { ++polls; m_lastError = "timeout"; return false; }
    void disconnect() override { ++disconnects; }
};
static QByteArray echo(QByteArray prefix) {
    const auto crc = Modbus_RTU_Protocol::calcCRC(prefix);
    prefix += char(crc); prefix += char(crc >> 8); return prefix;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        const QString test = argc > 1 ? argv[1] : "";
        if (test == "connection_state") {
            acResult = {nullptr, nullptr, false, "viOpen failed"};
            dcResult.success = false;
            dcResult.errorMessage = "viOpen failed";
            relayResult.success = false;
            relayResult.errorMessage = "open failed";
            const auto ac = InstrumentExecutor::runInput({}, InputRow{"1phase", "110", "60", "0"}, InputAction::PowerOn);
            const auto load = InstrumentExecutor::runLoad({}, {}, 0, {}, LoadAction::LoadOn);
            const auto dynamic = InstrumentExecutor::runDynamicLoad({}, {}, 0, {}, DynamicLoadAction::LoadOn);
            const auto relay = InstrumentExecutor::runRelay({}, {}, 0, RelayAction::RelayOn);
            for (const auto& result : {ac, load, dynamic, relay})
                require(!result.success && result.outputUnchanged && !result.confirmedOutput.has_value(),
                        "connection failure was treated as an output command");
        } else if (test == "static_validation") {
            Load load; load.setChannelIndex(1); load.setRealChannel(1);
            dcResult.success = true; dcResult.dcLoads = {&load};
            LoadDataRow row; row.values = {"1"};
            LoadMetaRow meta;
            for (const QString& mode : {"CR", "CP", "invalid"}) {
                meta.modes = {mode};
                const auto result = InstrumentExecutor::runLoad({}, {row}, 0, meta, LoadAction::LoadOn);
                require(!result.success && load.on == 0 && load.programmed == 0, "unsupported mode enabled load");
            }
            meta.modes = {"CC"};
            for (const QString& invalid : {"nan", "inf", "-1", "bad"}) {
                row.values = {invalid};
                require(!InstrumentExecutor::runLoad({}, {row}, 0, meta, LoadAction::LoadOn).success,
                    "invalid current accepted");
                require(load.on == 0, "invalid current enabled load");
            }
            meta.modes = {"CV"}; row.values = {"/12V/1A/"};
            require(!InstrumentExecutor::runLoad({}, {row}, 0, meta, LoadAction::LoadOn).success,
                "malformed CV accepted");
            mustFail([&] { InstrumentExecutor::applyLoadVonSetting(&load, 0, {"1"}); });
            mustFail([&] { InstrumentExecutor::applyLoadValueSettings(&load, 0, 1, "CC", {}, {}); });
            meta.modes = {"CC"}; row.values = {"1A"};
            require(InstrumentExecutor::runLoad({}, {row}, 0, meta, LoadAction::LoadOn).success
                && load.on == 1 && load.programmed == 1, "valid CC changed");
        } else if (test == "write") {
            for (int limit : {-1, 0, 1}) {
                Comm comm; comm.reply = "OFF"; comm.writeLimit = limit; if (limit == -1) comm.failOn = "VOLT";
                Chroma61505 source(&comm); source.connect();
                acResult = {&source, &comm, true};
                const auto result = InstrumentExecutor::runInput({}, InputRow{"1phase", "110", "60", "0"}, InputAction::PowerOn);
                require(!result.success, "executor accepted failed AC setting");
                require(result.outputUnchanged, "failed AC setting claimed an ON attempt");
                require(!comm.writes.join('|').toUpper().contains("OUTPUT ON"), "ON followed failed voltage");
            }
            Comm comm; comm.failOn = "CURR";
            Chroma63600 load("63640-80-80", &comm); load.connect();
            StaticCurrentParam p; p.levels = {1}; p.loadMode = "CCH";
            mustFail([&] { load.setStaticCurrent(p); load.setLoadOn(); });
            require(!comm.writes.join('|').contains("LOAD ON"), "LOAD ON followed failed current");
            Comm offComm; offComm.failOn = "VOLT"; offComm.reply = "0";
            Chroma61505 source(&offComm); source.connect(); acResult = {&source, &offComm, true};
            const auto offResult = InstrumentExecutor::runInput({}, InputRow{"1phase", "110", "60", "0"}, InputAction::PowerOff);
            require(!offResult.success && offComm.writes.join('|').toUpper().contains("OUTPUT OFF"), "zero-volt failure skipped OFF");
            require(offResult.confirmedOutput == false, "verified OFF lost after voltage-setting failure");
            Comm onComm; onComm.reply = "OFF"; onComm.failOn = "OUTPUT ON";
            Chroma61505 uncertain(&onComm); uncertain.connect(); acResult = {&uncertain, &onComm, true};
            const auto onResult = InstrumentExecutor::runInput({}, InputRow{"1phase", "110", "60", "0"}, InputAction::PowerOn);
            require(!onResult.success && !onResult.outputUnchanged && !onResult.confirmedOutput.has_value(),
                    "failed ON write incorrectly restored OFF");
        } else if (test == "relay_ack") {
            Comm comm; Relay_RTU_4 relay(&comm); relay.connect();
            require(!relay.turnOn(1), "timeout accepted");
            const auto valid = echo(QByteArray::fromHex("01050000ff00"));
            comm.reply = valid; comm.readChunk = 2; require(relay.turnOn(1), "fragmented echo rejected");
            for (auto bad : {valid.left(3), echo(QByteArray::fromHex("02050000ff00")), echo(QByteArray::fromHex("010500010000")), QByteArray::fromHex("018502c351")}) {
                comm.reply = bad; require(!relay.turnOn(1), "invalid echo accepted");
            }
            auto badCrc = valid; badCrc[7] ^= 1; comm.reply = badCrc;
            require(!relay.turnOn(1), "CRC ignored");
            comm.reply = valid; comm.writeLimit = 1; require(!relay.turnOn(1), "short write accepted");
            comm.writeLimit = -1; comm.reply = echo(QByteArray::fromHex("010500000000"));
            require(relay.turnOff(1), "valid OFF rejected");
            comm.reply = echo(QByteArray::fromHex("010f00000004"));
            require(relay.setMultiChannels({1,3}, true), "valid multiple-coil echo rejected");
            comm.writeLimit=-1;comm.readChunk=2;
            comm.reply=echo(QByteArray::fromHex("01010101"));
            require(relay.getStatus(1)==RelayStatus::Closed,"fragmented status rejected");
            comm.reply=echo(QByteArray::fromHex("01010100"));
            require(relay.getStatus(1)==RelayStatus::Open,"OFF status failed");
        } else if (test == "relay_mapping") {
            class FailedFirst : public Relay {
            public:
                bool turnOff(int ch) override { off << ch;return ch!=1; }
            } failedFirst;
            InstrumentConfig cleanupConfig;cleanupConfig.channels.resize(2);
            cleanupConfig.channels[0].index=1;cleanupConfig.channels[1].index=2;
            cleanupConfig.channelNumbers={1,2};
            ConditionLookup::RelayDataResult cleanupData;cleanupData.targetStates={"OFF","OFF"};
            mustFail([&]{InstrumentExecutorInternal::executeRelayAction(&failedFirst,RelayAction::RelayOff,cleanupConfig,cleanupData);});
            require(failedFirst.off==QVector<int>({1,2}),"failed OFF skipped remaining relay channel");
            class StatusRelay : public Relay {
            public:
                RelayStatus status=RelayStatus::Open;
                bool turnOn(int) override {return true;}
                RelayStatus getStatus(int) override {return status;}
            } verified;
            cleanupConfig.channels.resize(1);cleanupData.targetStates={" on "};
            mustFail([&]{InstrumentExecutorInternal::executeRelayAction(&verified,RelayAction::RelayOn,cleanupConfig,cleanupData);});
            verified.status=RelayStatus::Closed;
            InstrumentExecutorInternal::executeRelayAction(&verified,RelayAction::RelayOn,cleanupConfig,cleanupData);
            cleanupData.targetStates={"invalid"};
            mustFail([&]{InstrumentExecutorInternal::executeRelayAction(&verified,RelayAction::RelayOn,cleanupConfig,cleanupData);});
            Relay relay;
            InstrumentConfig first, second; first.name = "Relay1"; second.name = "Relay2";
            first.type = second.type = "Relay"; first.enabled = second.enabled = true;
            first.channels.resize(1); second.channels.resize(1);
            first.channels[0].index = second.channels[0].index = 1;
            first.channelNumbers = {1}; second.channelNumbers = {4};
            InstrumentSetupConfig cfg; cfg.instruments = {first, second};
            relayResult.success = true; relayResult.relays = {&relay}; relayResult.configurations = {second};
            relayResult.errorMessage = "Relay1 open failed";
            RelayDataRow row; row.values = {"off"};
            auto result = InstrumentExecutor::runRelay(cfg, {row}, 0, RelayAction::RelayOff);
            require(!result.success && relay.off == QVector<int>{4}, "partial creation lost device identity or failure");
            relayResult.errorMessage.clear();
            second.channels.resize(4);
            for(auto& channel:second.channels)channel.index=0;
            second.channels[0].index=1;
            relayResult.configurations={second};relay.off.clear();
            require(InstrumentExecutor::runRelay(cfg,{row},0,RelayAction::RelayOn).success && relay.off==QVector<int>{4},
                    "unassigned relay channel rejected or actuated");
            relay.off.clear();
            require(InstrumentExecutor::runRelay(cfg,{row},0,RelayAction::RelayOff).success && relay.off==QVector<int>{4},
                    "unassigned channel broke RelayOff");
            for(int badIndex:{-1,2}) {
                second.channels[1].index=badIndex;relayResult.configurations={second};relay.off.clear();
                require(!InstrumentExecutor::runRelay(cfg,{row},0,RelayAction::RelayOn).success && relay.off.isEmpty(),
                        "invalid mapped channel caused partial control");
            }
        } else if (test == "off") {
            for (bool dynamic : {false, true}) {
                Load first, second; first.failOff = true;
                mustFail([&] {
                    if (dynamic) InstrumentExecutorInternal::executeDynamicLoadOutputStep({&first,&second}, {}, DynamicLoadAction::LoadOff);
                    else InstrumentExecutorInternal::executeStaticLoadOutputStep({&first,&second}, {}, LoadAction::LoadOff);
                });
                require(first.off == 1 && second.off == 1, "OFF skipped remaining channel");
                first.failSelect = true; second.off = 0;
                mustFail([&] { InstrumentExecutorInternal::executeStaticLoadOutputStep({&first,&second}, {}, LoadAction::LoadOff); });
                require(second.off == 1, "selection error skipped other channel");
            }
        } else if (test == "dynamic") {
            Load load;
            mustFail([&] { InstrumentExecutor::applyDyLoadValueSettings(nullptr, 1, "1", "10", {}, {}); });
            mustFail([&] { InstrumentExecutor::applyDyLoadValueSettings(&load, 0, "1", "10", {"12"}, {}); });
            for (const auto& value : {"", "bad", "1~", "1~2~3", "nan", "inf", "-1~2"})
                mustFail([&] { InstrumentExecutor::applyDyLoadValueSettings(&load, 1, value, "10~20", {}, {}); });
            for (const auto& time : {"", "bad", "1~", "0", "-1", "nan", "inf", "1~2~3"})
                mustFail([&] { InstrumentExecutor::applyDyLoadValueSettings(&load, 1, "1~2", time, {}, {}); });
            require(load.programmed == 0, "invalid dynamic settings programmed hardware");
            InstrumentExecutor::applyDyLoadValueSettings(&load, 1, "1~2", "10~20", {}, {});
            require(load.parameter.levels == QVector<double>{1,2} && load.parameter.timings == QVector<double>{.01,.02}, "valid settings altered");
        } else if (test == "poll") {
            Scope scope; QWidget page; MSOSeries456TriggerController controller(&page);
            controller.setInstrument(&scope); controller.setSuspended(true); page.setEnabled(false);
            QEventLoop loop; QTimer::singleShot(650, &loop, &QEventLoop::quit); loop.exec();
            QMetaObject::invokeMethod(&controller, "updateRunStopStatus", Qt::DirectConnection);
            require(scope.polls == 0 && scope.disconnects == 0, "suspended scope still accessed");
            controller.setSuspended(false);
            QMetaObject::invokeMethod(&controller, "updateRunStopStatus", Qt::DirectConnection);
            require(scope.polls == 0, "unleased poll must not touch instrument");
        } else if (test == "async_poll") {
            class SlowScope : public Scope {
            public:
                bool isRunning() override { QThread::msleep(200); return Scope::isRunning(); }
            };
            auto scope=std::make_shared<SlowScope>();
            QWidget page;
            auto controller=std::make_unique<MSOSeries456TriggerController>(&page);
            controller->setInstrument(scope.get());
            controller->setPollingLeaseFactory([scope] { return scope; });
            QElapsedTimer timer;timer.start();
            QMetaObject::invokeMethod(controller.get(), "updateRunStopStatus", Qt::DirectConnection);
            require(timer.elapsed()<100 && controller->isOperationActive(),"poll blocked UI");
            QEventLoop loop;bool responsive=false;
            QTimer::singleShot(10,&loop,[&] { responsive=true; loop.quit(); });loop.exec();
            require(responsive && timer.elapsed()<150,"UI event loop blocked");
            std::weak_ptr<SlowScope> weak=scope;
            controller.reset();scope.reset();
            require(!weak.expired(),"scope destroyed during query");
            QTimer::singleShot(350,&loop,&QEventLoop::quit);loop.exec();
            require(weak.expired(),"scope leaked after query");
        } else if (test == "manual_async" || test == "manual_destroy") {
            class SlowMeasure : public Scope {
            public:
                std::atomic<int> reads{0};
                double measureSignalPeak(int, const QString&) override {
                    QThread::msleep(80); ++reads; return 3.5;
                }
            };
            auto scope = std::make_shared<SlowMeasure>();
            QWidget page;
            auto controller = std::make_unique<MSOSeries456TriggerController>(&page);
            controller->setInstrument(scope.get());
            controller->setPollingLeaseFactory([scope] { return scope; });
            QElapsedTimer timer; timer.start();
            QMetaObject::invokeMethod(controller.get(), "onGetValueTriggered", Qt::DirectConnection);
            require(timer.elapsed() < 100 && controller->hasActiveCommand(), "manual read blocked UI");
            QMetaObject::invokeMethod(controller.get(), "onGetValueTriggered", Qt::DirectConnection);
            if (test == "manual_destroy") controller.reset();
            QEventLoop loop; bool responsive = false;
            QTimer::singleShot(20, [&] { responsive = true; });
            QTimer::singleShot(420, &loop, &QEventLoop::quit); loop.exec();
            require(responsive && scope->reads == 4, "duplicate command or incomplete async read");
            if (controller) require(!controller->hasActiveCommand(), "manual command remained busy");
        } else if (test == "step_scale" || test == "step_scale_destroy") {
            testTriggerStep(test == "step_scale_destroy");
        } else if (test == "channels") {
            for (auto name : {"MSO44B", "MSO46B", "MSO58B", "MSO64B"}) {
                const int count = TriggerModelCatalog::channelCount(name);
                MSOSeries456TriggerWidget widget(count);
                auto* source = widget.findChild<QComboBox*>("triggerSource");
                require(source && source->count() == count, "wrong source count");
                require(count == (QString(name) == "MSO46B" ? 6 : QString(name) == "MSO58B" ? 8 : 4), "model count incorrect");
            }
        } else throw std::runtime_error("unknown scenario");
        std::cout << "PASS " << test.toStdString() << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
