#include "connectiontestservice.h"
#include "../../src/views/instrumentsetup/dialogs/communicationconfigdialog.h"
#include "scopesessioncoordinator.h"
#include "oscilloscope.h"
#include "outputoperationstate.h"
#include "oscilloscopesettingrules.h"
#include "../support/configuration_test_helpers.h"
#include "testsequencesettingsvalidation.h"
#include "../../src/views/instrumentsetup/instrumentsetuppage.h"
#include <QTableWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QThreadPool>
#include <QTimer>
#include <atomic>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void until(F ready) {
    QElapsedTimer t; t.start();
    while (!ready() && t.elapsed() < 4500) { QCoreApplication::processEvents(); QThread::msleep(1); }
    require(ready(), "async operation timed out");
}
struct ProbeState {
    QSemaphore entered, release;
    std::atomic<int> closed{0}, destroyed{0}, writes{0};
    std::atomic<bool> onGui{false};
    bool failOpen = false, throwRead = false, block = true;
};
class Connection : public ICommunication {
public:
    explicit Connection(std::shared_ptr<ProbeState> state) : state(std::move(state)) {}
    ~Connection() override { ++state->destroyed; }
    bool open() override {
        state->onGui = QThread::currentThread() == QCoreApplication::instance()->thread();
        state->entered.release();
        if (state->block && !state->release.tryAcquire(1, 3000)) throw std::runtime_error("test release timed out");
        return !state->failOpen;
    }
    void close() override { ++state->closed; }
    bool isOpen() const override { return true; }
    int write(const QByteArray &data) override { require(data == "*IDN?\n", "identity SCPI changed"); ++state->writes; return data.size(); }
    int read(QByteArray &data, int) override {
        if (state->throwRead) throw std::runtime_error("read failure");
        data = "offline-device\n"; return data.size();
    }
    QString lastError() const override { return "open failure"; }
    std::shared_ptr<ProbeState> state;
};
std::unique_ptr<ConnectionTestService> probe(const std::shared_ptr<ProbeState> &state) {
    return std::make_unique<ConnectionTestService>([state](const QString &) { return std::make_unique<Connection>(state); });
}
std::unique_ptr<ConnectionTestService> makeConnectionTestService() {
    return std::make_unique<ConnectionTestService>([](const QString&) -> std::unique_ptr<ICommunication> {
        throw std::runtime_error("must inject offline connection");
    });
}

void connectionTest() {
    auto state = std::make_shared<ProbeState>();
    auto service = probe(state);
    ConnectionTestResult received; bool delivered = false, heartbeat = false;
    QObject::connect(service.get(), &ConnectionTestService::completed, [&](const auto &r) { received=r; delivered=true; });
    require(service->start("GPIB0::1::INSTR"), "start rejected");
    require(state->entered.tryAcquire(1, 2000), "worker not entered");
    require(!service->start("second"), "overlapping probe accepted");
    QTimer::singleShot(0, [&] { heartbeat = true; }); until([&] { return heartbeat; });
    require(!state->onGui && !delivered, "probe blocked GUI or delivered early");
    state->release.release(); until([&] { return delivered; });
    require(received.opened && received.identity == "offline-device" && received.error.isEmpty() &&
            state->closed == 1 && state->destroyed == 1, "result or cleanup wrong");
    for (int failure : {0, 1}) {
        auto failed = std::make_shared<ProbeState>(); failed->block = false;
        failed->failOpen = failure == 0; failed->throwRead = failure == 1;
        auto operation = probe(failed); bool done = false;
        QObject::connect(operation.get(), &ConnectionTestService::completed, [&](const auto &r) {
            require(!r.error.isEmpty(), "failure lost"); done = true;
        });
        operation->start("offline"); until([&] { return done; });
        require(failed->closed == 1 && failed->destroyed == 1, "exception leaked connection");
    }
    auto slow = std::make_shared<ProbeState>();
    auto dialog = std::make_unique<CommunicationConfigDialog>(probe(slow), nullptr);
    dialog->show();
    require(QMetaObject::invokeMethod(dialog.get(), "onTestConnection", Qt::DirectConnection), "test action missing");
    require(slow->entered.tryAcquire(1, 2000), "dialog probe not entered");
    QElapsedTimer t; t.start(); dialog.reset();
    require(t.elapsed() < 250 && slow->closed == 0, "closing dialog waited for communication");
    slow->release.release(); until([&] { return slow->destroyed == 1; });
    require(slow->closed == 1, "destroyed owner leaked worker connection");
}

struct ScopeState { QSemaphore entered, release; std::atomic<int> closed{0}; std::atomic<bool> gui{false}; bool slow=false; };
class Scope : public Oscilloscope {
public:
    explicit Scope(std::shared_ptr<ScopeState> state) : state(std::move(state)) {}
    QString model() const override { return "MSOSeries456"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString &, const QString &) override { return {}; }
    bool isConnected() const override { return state->closed == 0; }
    void disconnect() override {
        state->gui = QThread::currentThread() == QCoreApplication::instance()->thread();
        state->entered.release();
        if (state->slow && !state->release.tryAcquire(1, 4000)) throw std::runtime_error("cleanup timed out");
        ++state->closed;
    }
    std::shared_ptr<ScopeState> state;
};
InstrumentSetupConfig configuration() {
    InstrumentSetupConfig config; InstrumentConfig item; item.type="Oscilloscope"; item.modelName="MSO44B";
    config.instruments.append(item); return config;
}
void retirementTest() {
    auto old = std::make_shared<ScopeState>(), next = std::make_shared<ScopeState>(); old->slow=true;
    std::atomic<int> opens{0}; std::atomic<bool> premature{false};
    auto owner = std::make_unique<ScopeSessionCoordinator>([&](const auto &) {
        if (++opens == 1) return OscilloscopeManager::OscMap{{"MSO44B",std::make_shared<Scope>(old)}};
        premature = old->closed != 1;
        return OscilloscopeManager::OscMap{{"MSO44B",std::make_shared<Scope>(next)}};
    });
    owner->queueConfiguration(configuration()); until([&] { return bool(owner->current()); });
    owner->queueConfiguration(configuration());
    until([&] { return old->entered.available() != 0; });
    bool heartbeat=false; QTimer::singleShot(0,[&] { heartbeat=true; }); until([&] { return heartbeat; });
    require(opens == 1 && !old->gui, "new connection raced cleanup or cleanup ran on GUI");
    old->release.release(); until([&] { return opens == 2 && !owner->isConfigurationBusy(); });
    require(!premature, "reconfiguration did not await retirement");
    next->slow=true;
    QElapsedTimer t; t.start(); owner.reset();
    require(t.elapsed()<250, "owner destructor blocked on disconnect");
    until([&] { return next->entered.available() != 0; });
    next->release.release(); until([&] { return next->closed==1; });
    require(!next->gui, "destructor cleanup ran on GUI");

    // Removing/re-adding a scope during a slow connection must not detach a future continuation.
    auto stale=std::make_shared<ScopeState>(); stale->slow=true;
    QSemaphore connecting, finishConnect;
    opens=0; premature=false;
    owner=std::make_unique<ScopeSessionCoordinator>([&](const auto &) {
        if (++opens==1) {
            connecting.release();
            require(finishConnect.tryAcquire(1,4000),"connection test timed out");
            return OscilloscopeManager::OscMap{{"MSO44B",std::make_shared<Scope>(stale)}};
        }
        premature=stale->closed!=1;
        return OscilloscopeManager::OscMap{{"MSO44B",std::make_shared<Scope>(std::make_shared<ScopeState>())}};
    });
    owner->queueConfiguration(configuration()); until([&] { return connecting.available()!=0; });
    owner->queueConfiguration({}); owner->queueConfiguration(configuration());
    finishConnect.release(); until([&] { return stale->entered.available()!=0; });
    require(!owner->current(),"obsolete connection adopted");
    stale->release.release(); until([&] { return opens==2 && bool(owner->current()); });
    require(!premature,"stale connection cleanup raced replacement");
    owner.reset();

    opens=0;
    owner=std::make_unique<ScopeSessionCoordinator>([&](const auto &) {
        if (++opens==1) throw std::runtime_error("open failure");
        return OscilloscopeManager::OscMap{{"MSO44B",std::make_shared<Scope>(std::make_shared<ScopeState>())}};
    });
    owner->queueConfiguration(configuration()); until([&] { return opens==1 && !owner->isConfigurationBusy(); });
    owner->queueConfiguration(configuration()); until([&] { return bool(owner->current()); });
    require(opens==2,"failed connection poisoned retirement chain");
    owner.reset();
}
void rulesTest() {
    const QList<QPair<QString,QVariant>> fields{
        {"timescale","20ms"},{"timescale","bad"},{"timescale",20},{"timescale","1e309ms"},
        {"sample_rate","6.25GS/s"},{"sample_rate","1V"},{"record_length","0.5k"},{"record_length","1.5"},
        {"horz_pos",100},{"horz_pos",101},{"horz_pos","50"},{"acq_mode","HIRes"},{"acq_mode","WFMDB"},
        {"ch1_scale","2mA"},{"ch1_scale","0V"},{"ch1_position",-8},{"ch1_position",8.1},
        {"ch1_coupling","DCR"},{"ch1_coupling","GND"},{"ch1_termination","1MΩ"},
        {"ch1_bandwidth","Full"},{"ch1_bandwidth","200MHz"},{"ch1_bandwidth","nan"},
        {"trig_source","CH8"},{"trig_source","CH9"},{"trig_edge","Rising"},{"trig_edge","nonsense"},
        {"fastacq",true},{"fastacq","true"}};
    for (const auto &[key,value] : fields) {
        const bool common = OscilloscopeSettingRules::decode(key,QJsonValue::fromVariant(value)).has_value();
        require(TestSequenceSettingsValidation::oscilloscopeSetting(key,QJsonValue::fromVariant(value))==common,"XML rule drift");
        QStringList commands; QString error;
        // Check the selected independent field. The other horizontal field is
        // a saved preview and is intentionally not decoded at runtime.
        const QString adjustment=key=="record_length" ? "HORIZONTALSCALE" : "RECORDLENGTH";
        const bool runtime=buildOscilloscopeCommands({{"horz_mode","MANUAL"},{"horz_config",adjustment},
            {"ch1_enabled",true},{key,value}},"MSOSeries456",8,commands,error);
        require(runtime==common,qPrintable("runtime rule drift: "+key+"="+value.toString()));
    }
    QStringList commands; QString error;
    for (const auto &model : {"DPO4000","DPO7000"})
        require(!buildOscilloscopeCommands({},model,4,commands,error) && commands.isEmpty(),"removed model accepted");
    require(buildOscilloscopeCommands({{"ignore_channels",true},{"ch1_scale","bad"}},"MSOSeries456",4,commands,error),"ignored section parsed");
    require(!OscSharedData::ACQ_MODES.contains("WFMDB"),"removed UI choice remains");
}
void outputTest() {
    OutputOperationState state;
    auto first=state.begin({{TableKind::Input,0}},true);
    auto second=state.begin({{TableKind::Input,0}},false);
    require(state.state(TableKind::Input)==OutputState::Unknown,"pending output claimed confirmed");
    state.complete(first,{});
    require(state.state(TableKind::Input)==OutputState::Unknown,"queued write lost uncertainty");
    state.complete(second,{false,"validation",true,{}});
    require(state.state(TableKind::Input)==OutputState::ConfirmedOn,"queued unchanged failure lost last settled result");
    auto off=state.begin({{TableKind::Input,0}},false);
    state.complete(off,{false,"failed",false,{}});
    require(state.state(TableKind::Input)==OutputState::Unknown && state.hasActiveControl(),"uncertain OFF released ownership");
    off=state.begin({{TableKind::Input,0}},false);
    state.complete(off,{false,"partial",false,false});
    require(!state.hasActiveControl(),"confirmed OFF failed to release ownership");
    auto grouped=state.begin({{TableKind::Dc,0},{TableKind::Dc,1}},true);
    require(state.busy(TableKind::Dc,1) && !state.busy(TableKind::Dc,2),"group targets mixed");
    state.complete(grouped,{false,"unchanged",true,{}});
    state.complete(grouped,{});
    require(!state.hasActiveControl(),"group failure or repeated completion corrupted state");
}
void serialSettingsTest() {
    const QStringList resources{
        "MODBUS::COM4:9600:8:N:1::SLAVE:1",
        "MODBUS::COM14:14400:7:E:2::SLAVE:17",
        "MODBUS::/dev/ttyUSB0:19200:8:O:1::SLAVE:3",
        "MODBUS::COM11:19200:5:M:1.5::SLAVE:9",
        "COM4:115200:8:N:1:HW",
        "COM14:14400:7:E:2:SW"
    };
    for (const auto &resource : resources) {
        auto config = CommunicationConfig::fromResourceString(resource);
        require(config.isValid() && config.toResourceString() == resource,
                "serial resource parser lost parameters");
        for (int reopen = 0; reopen < 3; ++reopen) {
            auto service = std::make_unique<ConnectionTestService>([](const QString&) -> std::unique_ptr<ICommunication> {
                throw std::runtime_error("settings roundtrip must not open hardware");
            });
            CommunicationConfigDialog dialog(std::move(service), nullptr);
            auto* port = dialog.findChild<QComboBox*>(config.protocol == ProtocolType::Serial ? "serialPort" : "modbusRtuPort");
            require(port, "serial port editor missing");
            // A saved/custom port need not be the enumerated port at currentIndex().
            port->clear();
            port->addItem("COM2 - Offline fixture", "COM2");
            dialog.setConfig(config);
            config = dialog.getConfig();
            require(config.toResourceString() == resource, "reopening settings changed the selected serial port");
            port->setEditText("COM42");
            require(dialog.getConfig().portName == "COM42", "typed port was replaced by stale itemData");
            port->setEditText("COM");
            require(!dialog.getConfig().isValid(), "incomplete COM port accepted");
            dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            require(dialog.result() != QDialog::Accepted, "OK accepted an incomplete COM port");
            dialog.setConfig(config);
            if (config.protocol == ProtocolType::Serial) {
                QMetaObject::invokeMethod(&dialog, "onRefreshSerialPorts", Qt::DirectConnection);
                require(dialog.getConfig().toResourceString() == resource, "port refresh changed configuration");
            }
            dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            require(dialog.result() == QDialog::Accepted, "valid serial settings rejected");
            config = CommunicationConfig::fromResourceString(resource);
        }
    }
    const auto tcp = CommunicationConfig::fromResourceString("MODBUS::192.168.1.10:502::SLAVE:7");
    require(tcp.toResourceString() == "MODBUS::192.168.1.10:502::SLAVE:7", "Modbus TCP parser lost endpoint");
    require(!CommunicationConfig::fromResourceString("COM1:9600:8:N:1::SLAVE:1").isValid(),
            "malformed Modbus address was accepted as valid serial settings");
}
void instrumentSetupSerialTest() {
    InstrumentSetupModel model;
    InstrumentSetupConfig config;
    InstrumentConfig relay;
    relay.name = "Relay1"; relay.type = "Relay"; relay.enabled = true;
    const QString original = "MODBUS::COM4:9600:8:N:1::SLAVE:1";
    relay.commConfig = CommunicationConfig::fromResourceString(original);
    config.instruments.append(relay);
    model.setConfig(config);
    InstrumentSetupViewModel vm(&model);
    InstrumentSetupPage page(&vm);
    auto* table = page.findChild<QTableWidget*>();
    require(table && table->rowCount() >= 1, "InstrumentSetup relay row missing");
    auto* address = qobject_cast<QComboBox*>(table->cellWidget(0, 2));
    auto* gear = qobject_cast<QPushButton*>(table->cellWidget(0, 3));
    require(address && gear, "InstrumentSetup address/settings controls missing");
    for (const QString expected : {original, QString("MODBUS::COM14:19200:7:E:2::SLAVE:17")}) {
        address->setEditText(expected);
        page.syncUIToViewModel();
        require(vm.currentConfig().instruments[0].getResourceString() == expected, "InstrumentSetup cached address overrode edited address");
        for (int reopen = 0; reopen < 3; ++reopen) {
            QTimer::singleShot(0, [&] {
                auto* dialog = qobject_cast<CommunicationConfigDialog*>(QApplication::activeModalWidget());
                require(dialog && dialog->getConfig().toResourceString() == expected, "gear reopened with wrong address");
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            gear->click();
            require(address->currentText() == expected && vm.currentConfig().instruments[0].getResourceString() == expected,
                    "gear + OK altered the relay resource");
        }
    }
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    try {
        const QString mode=argc>1?argv[1]:"";
        if(mode=="connection") connectionTest();
        else if(mode=="retirement") retirementTest();
        else if(mode=="rules") rulesTest();
        else if(mode=="outputs") outputTest();
        else if(mode=="serial") serialSettingsTest();
        else if(mode=="instrument_setup_serial") instrumentSetupSerialTest();
        else throw std::runtime_error("unknown scenario");
        require(QThreadPool::globalInstance()->waitForDone(5000),"worker leaked");
        return 0;
    } catch(const std::exception &error) { std::cerr<<error.what(); return 1; }
}
