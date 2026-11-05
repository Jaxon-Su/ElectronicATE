#include "connectiontestservice.h"
#include "acsourcefactory.h"
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
#include <QDir>
#include <atomic>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void snapshot(QWidget& widget, const QString& name) {
    const auto directory = qEnvironmentVariable("ELECTRONICATE_TEST_EVIDENCE_DIR");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    QCoreApplication::processEvents();
    require(widget.grab().save(QDir(directory).filePath(name + ".png")), "snapshot failed");
}
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
    std::atomic<int> reads{0};
    std::atomic<int> timeoutAtOpen{0};
};
class Connection : public ICommunication {
public:
    explicit Connection(std::shared_ptr<ProbeState> state) : state(std::move(state)) {}
    ~Connection() override { ++state->destroyed; }
    bool open() override {
        state->timeoutAtOpen = timeout;
        state->onGui = QThread::currentThread() == QCoreApplication::instance()->thread();
        state->entered.release();
        if (state->block && !state->release.tryAcquire(1, 3000)) throw std::runtime_error("test release timed out");
        return !state->failOpen;
    }
    void close() override { ++state->closed; }
    bool isOpen() const override { return true; }
    int write(const QByteArray &data) override { require(data == "*IDN?\n", "identity SCPI changed"); ++state->writes; return data.size(); }
    int read(QByteArray &data, int) override {
        ++state->reads;
        if (state->throwRead) throw std::runtime_error("read failure");
        data = "offline-device\n"; return data.size();
    }
    QString lastError() const override { return "open failure"; }
    void setTimeoutMs(int value) override { timeout = value; }
    int timeoutMs() const override { return timeout; }
    int timeout = 3000;
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
    auto rtu = std::make_shared<ProbeState>(); rtu->block = false;
    auto rtuService = probe(rtu);
    bool rtuDone = false;
    QObject::connect(rtuService.get(), &ConnectionTestService::completed, [&](const auto& result) {
        require(result.opened && result.identity.isEmpty() && result.error.isEmpty(), "RTU open-only result wrong");
        rtuDone = true;
    });
    require(rtuService->start("MODBUS::COM4:9600:8:N:1::SLAVE:1"), "RTU probe rejected");
    until([&] { return rtuDone; });
    require(rtu->writes == 0 && rtu->reads == 0 && rtu->closed == 1 && rtu->destroyed == 1, "RTU probe sent SCPI or leaked transport");
    require(!rtuService->start("MODBUS::COM4:9600:7:N:1::SLAVE:1") &&
            !rtuService->start("MODBUS::COM4:9600:8:N:1::SLAVE:248"), "invalid RTU probe accepted");
    auto tcpState = std::make_shared<ProbeState>(); tcpState->block = false;
    auto tcpService = probe(tcpState); bool tcpDone = false;
    QObject::connect(tcpService.get(), &ConnectionTestService::completed, [&](const auto& result) {
        require(result.opened && !result.transportOnly && result.identity == "offline-device", "existing Modbus TCP probe changed");
        tcpDone = true;
    });
    require(tcpService->start("MODBUS::192.168.1.100:502::SLAVE:1"), "RTU restriction rejected Modbus TCP");
    until([&] { return tcpDone; });
    require(!rtuService->start("MODBUS::COM4:9600:8:N:1::SLAVE:1", 0), "nonpositive timeout accepted");
    for (int change = 0; change < 3; ++change) {
        auto s = std::make_shared<ProbeState>();
        auto service = probe(s); bool completed = false;
        QObject::connect(service.get(), &ConnectionTestService::completed, [&](const auto&) { completed = true; });
        CommunicationConfigDialog dialog(std::move(service), nullptr);
        dialog.setConfig(CommunicationConfig::fromResourceString("MODBUS::COM4:9600:8:N:1::SLAVE:1"));
        QMetaObject::invokeMethod(&dialog, "onTestConnection", Qt::DirectConnection);
        require(s->entered.tryAcquire(1, 2000), "RTU stale probe did not start");
        if (change == 1) {
            auto* protocol = dialog.findChild<QComboBox*>("communicationProtocol");
            protocol->setCurrentIndex(protocol->findData(int(ProtocolType::Serial)));
        } else if (change == 0) {
            dialog.findChild<QSpinBox*>("modbusSlaveId")->setValue(2);
        } else {
            auto updated = dialog.getConfig(); updated.timeout = 2345; dialog.setConfig(updated);
        }
        s->release.release(); until([&] { return completed; });
        require(dialog.findChild<QLabel*>("connectionTestStatus")->text().isEmpty(), "obsolete RTU result shown for new settings");
        require(s->closed == 1 && s->writes == 0 && s->reads == 0, "stale RTU probe cleanup wrong");
    }
    {
        auto s = std::make_shared<ProbeState>(); s->block = false;
        CommunicationConfigDialog dialog(probe(s), nullptr);
        auto config = CommunicationConfig::fromResourceString("COM42:14400:7:E:2:HW"); config.timeout = 2345;
        dialog.setConfig(config);
        QMetaObject::invokeMethod(&dialog, "onTestConnection", Qt::DirectConnection);
        until([&] { return s->destroyed == 1; });
        require(s->timeoutAtOpen == 2345 && s->writes == 1 && s->reads == 1 && s->closed == 1,
                "Serial probe timeout, IDN or cleanup changed");
    }
    for (bool fail : {false, true}) {
        auto s = std::make_shared<ProbeState>(); s->block = false; s->failOpen = fail;
        CommunicationConfigDialog dialog(probe(s), nullptr);
        auto config = CommunicationConfig::fromResourceString("MODBUS::COM4:9600:8:N:1::SLAVE:1");
        config.timeout = 1234;
        dialog.setConfig(config);
        dialog.show();
        QMetaObject::invokeMethod(&dialog, "onTestConnection", Qt::DirectConnection);
        auto* status = dialog.findChild<QLabel*>("connectionTestStatus");
        until([&] { return status && (status->text().contains("未驗證設備回應") || status->text().contains("連線失敗")); });
        require(status->text().contains(fail ? "連線失敗" : "串口已開啟，未驗證設備回應"), "RTU status misrepresents probe");
        require(s->writes == 0 && s->reads == 0 && s->closed == 1 && s->destroyed == 1, "RTU dialog probe cleanup wrong");
        require(s->timeoutAtOpen == 1234, "Dialog timeout did not reach transport before open");
    }
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
    for (const QString resource : {QString("GPIB0::1::INSTR"), QString("MODBUS::COM4:9600:8:N:1::SLAVE:1")}) {
    auto slow = std::make_shared<ProbeState>();
    auto dialog = std::make_unique<CommunicationConfigDialog>(probe(slow), nullptr);
    dialog->setConfig(CommunicationConfig::fromResourceString(resource));
    dialog->show();
    require(QMetaObject::invokeMethod(dialog.get(), "onTestConnection", Qt::DirectConnection), "test action missing");
    require(slow->entered.tryAcquire(1, 2000), "dialog probe not entered");
    QElapsedTimer t; t.start(); dialog.reset();
    require(t.elapsed() < 250 && slow->closed == 0, "closing dialog waited for communication");
    slow->release.release(); until([&] { return slow->destroyed == 1; });
    require(slow->closed == 1, "destroyed owner leaked worker connection");
    if (resource.startsWith("MODBUS")) require(slow->writes == 0 && slow->reads == 0, "closed RTU probe performed IO");
    }
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
void tcpSettingsTest() {
    CommunicationConfigDialog dialog;
    auto config = CommunicationConfig::fromResourceString("TCPIP0::192.168.1.100::INSTR");
    dialog.setConfig(config);
    dialog.show();
    QCoreApplication::processEvents();
    snapshot(dialog, "vxi11");
    for (auto* spin : dialog.findChildren<QSpinBox*>())
        if (spin->maximum() == 65535 && spin->value() == 5025)
            require(!spin->isVisible(), "VXI-11 exposes an unused Port");
    config = CommunicationConfig::fromResourceString("TCPIP0::192.168.1.101::8462::SOCKET");
    dialog.setConfig(config);
    require(dialog.getConfig().toResourceString() == "TCPIP0::192.168.1.101::8462::SOCKET",
            "Socket was silently converted when reopening settings");
    auto* port = dialog.findChild<QSpinBox*>("tcpPort");
    auto* protocols = dialog.findChild<QComboBox*>("communicationProtocol");
    auto* hint = dialog.findChild<QLabel*>("tcpHint");
    auto* preview = dialog.findChild<QLabel*>("tcpResourcePreview");
    require(port && protocols && hint && preview && port->isVisible(), "Socket Port not visible");
    snapshot(dialog, "socket");
    auto* form = qobject_cast<QFormLayout*>(port->parentWidget()->layout());
    require(form && form->labelForField(port)->isVisible(), "Socket Port label missing");
    port->setValue(3300);
    require(preview->text() == "TCPIP0::192.168.1.101::3300::SOCKET", "Socket preview stale");
    protocols->setCurrentIndex(protocols->findData(int(ProtocolType::TCP_VXI11)));
    require(!port->isVisible() && !form->labelForField(port)->isVisible() &&
            !hint->text().contains("8462") && !hint->text().contains("3300") &&
            preview->text() == "TCPIP0::192.168.1.101::INSTR", "VXI-11 leaked Socket settings");
    protocols->setCurrentIndex(protocols->findData(int(ProtocolType::TCP_Socket)));
    require(dialog.getConfig().toResourceString() == "TCPIP0::192.168.1.101::3300::SOCKET", "TCP switching lost draft");
    for (const QString resource : {QString("TCPIP0::192.168.1.101::3300::SOCKET"),
                                  QString("TCPIP0::192.168.1.100::INSTR"),
                                  QString("TCPIP0::192.168.1.100::hislip0::INSTR")}) {
        auto cfg = CommunicationConfig::fromResourceString(resource); cfg.timeout = 1234;
        dialog.setConfig(cfg);
        QString xml; QXmlStreamWriter writer(&xml); dialog.getConfig().writeXml(writer);
        QXmlStreamReader reader(xml); require(reader.readNextStartElement(), "XML missing configuration");
        CommunicationConfig restored; restored.readXml(reader);
        require(!reader.hasError() && restored.toResourceString() == resource && restored.timeout == 1234, "TCP XML roundtrip lost settings");
        dialog.setConfig(restored);
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        require(dialog.result() == QDialog::Accepted && dialog.getConfig().toResourceString() == resource, "TCP OK changed protocol");
    }
    dialog.setAvailableProtocols({ProtocolType::TCP_Socket});
    require(protocols->count() == 1 && dialog.getConfig().protocol == ProtocolType::TCP_Socket, "restricted Socket unavailable");
}
void serialSettingsTest() {
    CommunicationConfigDialog shared;
    shared.setConfig(CommunicationConfig::fromResourceString("COM42:14400:7:E:2:HW"));
    shared.show();
    snapshot(shared, "serial");
    auto* commonPort = shared.findChild<QComboBox*>("serialPort");
    auto* protocols = shared.findChild<QComboBox*>("communicationProtocol");
    require(commonPort && protocols, "shared serial editors missing");
    protocols->setCurrentIndex(protocols->findData(int(ProtocolType::Modbus_RTU)));
    require(commonPort->isVisible() && shared.getConfig().portName == "COM42" &&
            shared.getConfig().baudRate == 14400, "RTU switched to a separate serial form");
    auto* flow = shared.findChild<QComboBox*>("serialFlowControl");
    auto* slave = shared.findChild<QSpinBox*>("modbusSlaveId");
    auto* bits = shared.findChild<QComboBox*>("serialDataBits");
    require(flow && slave && bits && !flow->isVisible() && slave->isVisible() &&
            bits->count() == 1 && shared.getConfig().dataBits == 8 && shared.getConfig().flowControl == "N", "RTU controls wrong");
    slave->setValue(247);
    snapshot(shared, "rtu");
    protocols->setCurrentIndex(protocols->findData(int(ProtocolType::Serial)));
    require(flow->isVisible() && !slave->isVisible() && shared.getConfig().toResourceString() == "COM42:14400:7:E:2:HW", "Serial draft lost");
    protocols->setCurrentIndex(protocols->findData(int(ProtocolType::Modbus_RTU)));
    require(shared.getConfig().toResourceString() == "MODBUS::COM42:14400:8:E:2::SLAVE:247", "RTU draft lost");
    for (const QString invalid : {QString("MODBUS::COM4:9600:7:N:1::SLAVE:1"),
                                  QString("MODBUS::COM4:9600:8:N:1::SLAVE:248"),
                                  QString("MODBUS::COM4:9600:8:N:1::SLAVE:0")}) {
        auto cfg = CommunicationConfig::fromResourceString(invalid);
        require(!cfg.isValid(), "invalid RTU parameters accepted");
        shared.setConfig(cfg);
        require(shared.getConfig().toResourceString() == invalid && !shared.getConfig().isValid(), "invalid legacy RTU silently corrected");
        shared.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        require(shared.result() != QDialog::Accepted, "invalid RTU OK accepted");
    }
    const QStringList resources{
        "MODBUS::COM4:9600:8:N:1::SLAVE:1",
        "MODBUS::COM14:14400:8:E:2::SLAVE:17",
        "MODBUS::/dev/ttyUSB0:19200:8:O:1::SLAVE:3",
        "MODBUS::COM11:19200:8:M:1.5::SLAVE:9",
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
            auto* port = dialog.findChild<QComboBox*>("serialPort");
            require(port, "serial port editor missing");
            // A saved/custom port need not be the enumerated port at currentIndex().
            port->clear();
            port->addItem("COM2 - Offline fixture", "COM2");
            dialog.setConfig(config);
            config = dialog.getConfig();
            require(config.toResourceString() == resource, "reopening settings changed the selected serial port");
            QString xml; QXmlStreamWriter writer(&xml); config.writeXml(writer);
            QXmlStreamReader reader(xml); reader.readNextStartElement();
            CommunicationConfig restored; restored.readXml(reader);
            require(!reader.hasError() && restored.toResourceString() == resource, "serial XML lost parameters");
            dialog.setConfig(restored);
            port->setEditText("COM42");
            require(dialog.getConfig().portName == "COM42", "typed port was replaced by stale itemData");
            port->setEditText("COM");
            require(!dialog.getConfig().isValid(), "incomplete COM port accepted");
            dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            require(dialog.result() != QDialog::Accepted, "OK accepted an incomplete COM port");
            dialog.setConfig(config);
            {
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
void slaveIdXmlTest() {
    for (const auto resource : {QString("MODBUS::COM42:14400:8:E:2::SLAVE:17"),
                               QString("MODBUS::192.168.1.100:502::SLAVE:17")}) {
        InstrumentConfig relay; relay.name = "Relay1"; relay.type = "Relay";
        relay.commConfig = CommunicationConfig::fromResourceString(resource); relay.commConfig.timeout = 1234;
        InstrumentSetupConfig config; config.instruments = {relay};
        InstrumentSetupModel model; model.setConfig(config);
        auto savedXml = [&] { QByteArray xml; QXmlStreamWriter w(&xml); model.writeXml(w); return xml; };
        const auto original = savedXml();
        for (const QByteArray replacement : {QByteArray{}, QByteArray("<SlaveId>17</SlaveId><SlaveId>2</SlaveId>"),
                                             QByteArray("<SlaveId>abc</SlaveId>")}) {
            auto damaged = original; damaged.replace("<SlaveId>17</SlaveId>", replacement);
            QXmlStreamReader r(damaged); r.readNextStartElement(); model.loadXml(r);
            require(r.hasError() && r.errorString().contains("SlaveId"), "malformed SlaveId XML silently accepted");
            require(savedXml() == original, "failed SlaveId XML load mutated model");
        }
    }
}
void acCatalogTest() {
    InstrumentSetupModel model;
    require(model.loadBaseXml(INSTRUMENT_CATALOG), "failure loading shipped AC catalog");
    InstrumentSetupViewModel vm(&model);
    InstrumentSetupPage page(&vm);
    const auto combo = [&]() -> QComboBox* {
        auto *table = page.findChild<QTableWidget*>();
        require(table, "missing Instrument Setup table");
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0) && table->item(row, 0)->text() == "ACSource")
                return qobject_cast<QComboBox*>(table->cellWidget(row, 1));
        return nullptr;
    };
    const auto names = model.getXmlModelMap().value("ACSource");
    require(!names.isEmpty() && combo() && combo()->count() == names.size()+1, "AC model dropdown/catalog mismatch");
    auto state = std::make_shared<ProbeState>();
    Connection transport(state);
    for (const QString &name : names) {
        require(!name.isEmpty() && names.count(name)==1 && combo()->findText(name)>=0, "missing/duplicate AC model");
        combo()->setCurrentText(name);
        page.syncUIToViewModel();
        QString saved; QXmlStreamWriter out(&saved); model.writeXml(out);
        QXmlStreamReader in(saved); in.readNextStartElement(); model.loadXml(in);
        require(!in.hasError() && combo() && combo()->currentText()==name, "saved catalog choice not restored in dropdown");
        QString selected;
        for (const auto &ic : vm.currentConfig().instruments)
            if (ic.name=="ACSource") selected=ic.modelName;
        std::unique_ptr<ACSource> driver(ACSourceFactory::createACSource(selected,&transport));
        require(driver && driver->model()==name, "selectable AC model lost its identity at the driver factory");
    }
    require(state->writes==0, "selecting or restoring an AC model wrote to hardware");
}

void loadFrameChoicesTest() {
    InstrumentSetupModel model;
    require(model.loadBaseXml(INSTRUMENT_CATALOG), "shipped Load catalog failed to load");
    InstrumentSetupViewModel vm(&model);
    InstrumentSetupPage page(&vm);
    auto *table = page.findChild<QTableWidget*>();
    require(table, "no instrument table");
    int row = 0;
    while (row < table->rowCount() && (!table->item(row,0) || table->item(row,0)->text() != "Load1")) ++row;
    require(row < table->rowCount(), "no Load1 row");
    const auto frame = [&]{return qobject_cast<QComboBox*>(table->cellWidget(row,1));};
    const auto box = [&](int ch,int offset=0) {
        int pos = vm.channels().indexOf(ch);
        require(pos>=0, "missing physical CH column");
        return qobject_cast<QComboBox*>(table->cellWidget(row+offset,4+pos));
    };
    const auto mapped = [&](int ch) {
        for (const auto &inst : vm.currentConfig().instruments)
            if (inst.name=="Load1") return inst.channels.value(inst.channelNumbers.indexOf(ch));
        throw std::runtime_error("no Load1 in configuration");
    };
    const auto roundTrip = [&](bool oldLayout = false) {
        QString xml; QXmlStreamWriter w(&xml); model.writeXml(w);
        if (oldLayout) {
            // Before CH10 was added, the last saved column was CH9. Do not shift those positions.
            QDomDocument legacy;
            require(bool(legacy.setContent(xml)), "cannot construct legacy layout fixture");
            legacy.documentElement().setAttribute("schemaVersion", "2");
            const auto channelLists = legacy.elementsByTagName("Channels");
            for (int i = 0; i < channelLists.count(); ++i) {
                auto channels = channelLists.at(i).toElement();
                for (auto ch = channels.firstChildElement("Channel"); !ch.isNull(); ) {
                    auto next = ch.nextSiblingElement("Channel");
                    const int number = ch.attribute("number").toInt();
                    if (number == 6 || number == 8 || number == 10) channels.removeChild(ch);
                    else ch.removeAttribute("number");
                    ch = next;
                }
            }
            xml = legacy.toString();
        }
        QXmlStreamReader in(xml); in.readNextStartElement(); model.loadXml(in);
        require(!in.hasError(), "Load frame XML rejected");
    };
    const QMap<QString,QSet<int>> frames{{"63600-1",{1,2}},{"63600-2",{1,2,3,4}},
                                         {"63600-5",{1,2,3,4,5,6,7,8,9,10}},{"63601-5",{1,3,5,7,9,10}}};
    for (auto it=frames.cbegin(); it!=frames.cend(); ++it) {
        require(frame() && frame()->findText(it.key())>=0, "frame missing in Load model menu");
        frame()->setCurrentText(it.key());
        for (const auto *module : {"63610-80-20","63630-80-60","63630-600-15","63640-80-80","63640-150-60"})
            require(box(1)->findText(module)>=0, "catalog omitted a supported module");
        for (int ch : vm.channels()) {
            for (int offset : {0,1,2})
                require(box(ch,offset) && box(ch,offset)->isEnabled()==it.value().contains(ch),
                        "module, output index or sync role enabled on wrong channel");
        }
    }
    frame()->setCurrentText("63600-2");
    box(4)->setCurrentText("63610-80-20"); box(4,1)->setCurrentText("1");
    roundTrip();
    require(mapped(4).index==1 && mapped(4).subModel=="63610-80-20" && mapped(3).index==0,
            "even channel or its independent index was lost");
    box(3)->setCurrentText("63630-80-60");
    require(!box(4)->isEnabled() && mapped(4).index==0 && mapped(4).subModel.isEmpty(),
            "single-channel module retained a second output");
    frame()->setCurrentText("63601-5");
    require(box(10)->count()==2 && box(10)->findText("63610-80-20")>=0 &&
            box(10)->findText("63640-80-80")<0,"CH10 offered a single-channel module");
    box(10)->setCurrentText("63610-80-20");
    box(10,1)->setCurrentText("1");
    box(10,2)->setCurrentText("SLAVE");
    roundTrip();
    require(frame()->currentText()=="63601-5" && mapped(10).subModel=="63610-80-20" &&
            mapped(10).index==1 && mapped(10).syncType==2,"CH10 was lost or renumbered in XML");
    box(9)->setCurrentText("63640-80-80");
    require(!box(10)->isEnabled() && !box(10,1)->isEnabled() && !box(10,2)->isEnabled() &&
            mapped(10).index==0 && mapped(10).subModel.isEmpty() && mapped(10).syncType==-1,
            "CH9 single-channel module retained an active CH10 mapping");
    box(9)->setCurrentText("63610-80-20");
    require(box(10)->isEnabled() && box(10)->currentText().isEmpty(), "CH10 was not re-enabled cleanly");
    frame()->setCurrentText("63600-5");box(9)->setCurrentText("63640-80-80");box(9,1)->setCurrentText("1");
    roundTrip(true);
    require(frame()->currentText()=="63600-5" && mapped(9).index==1 && !box(10)->isEnabled(),
            "existing five-slot configuration changed on round trip");
    frame()->setCurrentText("63600-1");
    require(!box(9)->isEnabled() && mapped(9).index==0 && mapped(9).subModel.isEmpty(),"smaller frame kept CH9");
    frame()->setCurrentText("6314A");
    require(box(1)->findText("63103") < 0 && box(2)->findText("63103A") < 0, "incompatible 6314A choice");
    box(7)->setCurrentText("63107A");
    require(box(8)->findText("63102A") < 0, "same physical slot offered a different module");
    box(8)->setCurrentText("63107A"); box(8,1)->setCurrentText("1");
    roundTrip();
    require(mapped(8).subModel == "63107A" && mapped(8).index == 1 && mapped(7).index == 0,
            "right output could not be independently mapped");
    box(1)->setCurrentText("63106A");
    require(!box(2)->isEnabled() && !box(3)->isEnabled() && !box(4)->isEnabled() && box(5)->isEnabled(),
            "wide module did not reserve its two slots");

    int peerRow = 0;
    while (peerRow < table->rowCount() && (!table->item(peerRow,0) || table->item(peerRow,0)->text() != "Load2")) ++peerRow;
    require(peerRow < table->rowCount(), "no Load2 row");
    const int ch1col = 4 + vm.channels().indexOf(1);
    const auto peerBox = [&](int offset,int col){return qobject_cast<QComboBox*>(table->cellWidget(peerRow+offset,col));};
    frame()->setCurrentText("63600-2");
    peerBox(0,1)->setCurrentText("63600-2");
    qobject_cast<QComboBox*>(table->cellWidget(row,2))->setCurrentText("GPIB0::8::INSTR");
    peerBox(0,2)->setCurrentText("GPIB0::8::INSTR");
    box(1)->setCurrentText("63610-80-20"); box(1,1)->setCurrentText("1");
    peerBox(0,ch1col)->setCurrentText("63610-80-20");
    require(peerBox(1,ch1col)->toolTip().contains("duplicate") && box(1,1)->toolTip().isEmpty(),
            "second row could claim an occupied physical channel or original owner was blocked");
    peerBox(0,2)->setCurrentText("GPIB0::9::INSTR");
    require(peerBox(1,ch1col)->toolTip().isEmpty(), "distinct instrument did not release physical channel lock");
    const int ch2col = 4 + vm.channels().indexOf(2);
    peerBox(0,ch1col)->setCurrentText("");
    peerBox(0,2)->setCurrentText("GPIB0::8::INSTR");
    page.findChild<QSpinBox*>("spinBox_Load_Outputs")->setValue(2);
    peerBox(0,ch2col)->setCurrentText("63610-80-20");
    peerBox(1,ch2col)->setCurrentText("2");
    peerBox(2,ch2col)->setCurrentText("SLAVE");
    require(peerBox(0,ch2col)->currentText()=="63610-80-20" && peerBox(1,ch2col)->currentText()=="2" &&
            peerBox(2,ch2col)->currentText()=="SLAVE", "cross-row second output was never configured");
    box(1)->setCurrentText("63630-80-60");
    require(!peerBox(0,ch2col)->isEnabled() && peerBox(0,ch2col)->currentText().isEmpty() &&
            peerBox(1,ch2col)->currentText().isEmpty() && peerBox(2,ch2col)->currentText().isEmpty(),
            "switching odd module retained another Load row's invalid even output");
    roundTrip();
    peerBox(0,2)->setCurrentText("GPIB0::9::INSTR");
    require(peerBox(0,ch2col)->isEnabled(), "separate chassis still inherited its peer's slot restriction");
}

void instrumentSetupSerialTest() {
    InstrumentSetupModel model;
    InstrumentSetupConfig config;
    InstrumentConfig relay;
    relay.name = "Relay1"; relay.type = "Relay"; relay.enabled = true;
    const QString original = "MODBUS::COM4:9600:8:N:1::SLAVE:1";
    relay.commConfig = CommunicationConfig::fromResourceString(original);
    relay.commConfig.timeout = 1234;
    config.instruments.append(relay);
    model.setConfig(config);
    InstrumentSetupViewModel vm(&model);
    InstrumentSetupPage page(&vm);
    auto* table = page.findChild<QTableWidget*>();
    require(table && table->rowCount() >= 1, "InstrumentSetup relay row missing");
    auto* address = qobject_cast<QComboBox*>(table->cellWidget(0, 2));
    auto* gear = qobject_cast<QPushButton*>(table->cellWidget(0, 3));
    require(address && gear, "InstrumentSetup address/settings controls missing");
    for (const QString expected : {original, QString("MODBUS::COM14:19200:8:E:2::SLAVE:17"),
                                  QString("COM42:14400:7:E:2:HW"), QString("TCPIP0::192.168.1.100::INSTR"),
                                  QString("TCPIP0::192.168.1.101::3300::SOCKET"),
                                  QString("TCPIP0::192.168.1.100::hislip0::INSTR")}) {
        address->setEditText("");
        require(vm.currentConfig().instruments[0].commConfig.timeout == 1234, "clearing address reset timeout");
        address->setEditText(expected);
        page.syncUIToViewModel();
        require(vm.currentConfig().instruments[0].commConfig.timeout == 1234, "address edit reset timeout");
        require(vm.currentConfig().instruments[0].getResourceString() == expected, "InstrumentSetup cached address overrode edited address");
        for (int reopen = 0; reopen < 3; ++reopen) {
            QTimer::singleShot(0, [&] {
                auto* dialog = qobject_cast<CommunicationConfigDialog*>(QApplication::activeModalWidget());
                require(dialog && dialog->getConfig().toResourceString() == expected, "gear reopened with wrong address");
                require(dialog->getConfig().timeout == 1234, "gear discarded saved timeout");
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            gear->click();
            require(address->currentText() == expected && vm.currentConfig().instruments[0].getResourceString() == expected,
                    "gear + OK altered the relay resource");
        }
        QByteArray xml; QXmlStreamWriter writer(&xml); model.writeXml(writer);
        QXmlStreamReader reader(xml); reader.readNextStartElement();
        InstrumentSetupModel restored; restored.loadXml(reader);
        require(!reader.hasError() && restored.getConfig().instruments[0].getResourceString() == expected,
                "InstrumentSetup XML changed communication settings");
        require(restored.getConfig().instruments[0].commConfig.timeout == 1234, "XML lost timeout");
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<CommunicationConfigDialog*>(QApplication::activeModalWidget());
            require(dialog, "cancel dialog missing");
            dialog->setConfig(CommunicationConfig::fromResourceString("COM99:115200:8:N:1:N"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
        });
        gear->click();
        require(address->currentText() == expected && vm.currentConfig().instruments[0].getResourceString() == expected,
                "Cancel committed edited configuration");
    }
}
void invalidRtuDraftTest() {
    for (int invalid = 0; invalid < 8; ++invalid) {
        InstrumentConfig relay;
        relay.name = "Relay1"; relay.type = "Relay";
        relay.commConfig = CommunicationConfig::fromResourceString("MODBUS::COM42:14400:8:E:2::SLAVE:17");
        if (invalid == 0) relay.commConfig.dataBits = 7;
        if (invalid == 1) relay.commConfig.slaveId = 0;
        if (invalid == 2) relay.commConfig.slaveId = 248;
        if (invalid == 3) relay.commConfig.slaveId = -1;
        if (invalid == 4) relay.commConfig.baudRate = 0;
        if (invalid == 5) relay.commConfig.parity = "invalid";
        if (invalid == 6) relay.commConfig.stopBits = "3";
        if (invalid == 7) relay.commConfig.portName = "";
        relay.commConfig.timeout = 1234;
        const auto resource = relay.commConfig.toResourceString();
        InstrumentSetupConfig config; config.instruments = {relay};
        InstrumentSetupModel source; source.setConfig(config);
        QByteArray xml; QXmlStreamWriter writer(&xml); source.writeXml(writer);
        InstrumentSetupModel model;
        QXmlStreamReader reader(xml); reader.readNextStartElement(); model.loadXml(reader);
        require(!reader.hasError(), "invalid integer RTU draft rejected instead of retained");
        InstrumentSetupViewModel vm(&model); InstrumentSetupPage page(&vm);
        auto* table = page.findChild<QTableWidget*>();
        int row = 0;
        while (row < table->rowCount() && (!table->item(row, 0) || table->item(row, 0)->text() != "Relay1")) ++row;
        require(row < table->rowCount(), "loaded Relay1 row missing");
        auto* address = qobject_cast<QComboBox*>(table->cellWidget(row, 2));
        auto* gear = qobject_cast<QPushButton*>(table->cellWidget(row, 3));
        require(address && gear, "loaded Relay1 controls missing");
        auto currentRelay = [&] {
            for (const auto& item : vm.currentConfig().instruments) if (item.name == "Relay1") return item;
            throw std::runtime_error("loaded Relay1 config missing");
        };
        auto unchanged = [&] {
            const auto current = currentRelay();
            require(current.getResourceString().isEmpty(), "invalid draft became executable");
            require(current.commConfig.toResourceString() == resource && current.commConfig.timeout == 1234,
                    "invalid RTU draft or timeout lost in page sync");
            require(address->currentText() == resource, "invalid RTU draft hidden from address");
        };
        page.syncUIToViewModel(); unchanged();
        for (bool correct : {false, true}) {
            QString dialogError;
            QTimer::singleShot(0, [&] {
                auto* dialog = qobject_cast<CommunicationConfigDialog*>(QApplication::activeModalWidget());
                try {
                require(dialog && dialog->getConfig().toResourceString() == resource && dialog->getConfig().timeout == 1234,
                        "gear discarded invalid RTU draft");
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                buttons->button(QDialogButtonBox::Ok)->click();
                require(dialog->result() != QDialog::Accepted, "invalid RTU accepted");
                QMetaObject::invokeMethod(dialog, "onTestConnection", Qt::DirectConnection);
                require(dialog->findChild<QLabel*>("connectionTestStatus")->text().contains("配置不完整"),
                        "invalid RTU test was not rejected");
                if (correct) {
                    auto fixed = CommunicationConfig::fromResourceString("MODBUS::COM42:14400:8:E:2::SLAVE:17");
                    fixed.timeout = dialog->getConfig().timeout;
                    dialog->setConfig(fixed); buttons->button(QDialogButtonBox::Ok)->click();
                    require(dialog->result() == QDialog::Accepted, "corrected RTU rejected");
                } else buttons->button(QDialogButtonBox::Cancel)->click();
                } catch (const std::exception& e) {
                    dialogError = QString::fromUtf8(e.what());
                    if (dialog) dialog->reject();
                }
            });
            gear->click();
            require(dialogError.isEmpty(), qPrintable(dialogError));
            if (!correct) {
                unchanged();
                QByteArray saved; QXmlStreamWriter w(&saved); model.writeXml(w);
                QXmlStreamReader r(saved); r.readNextStartElement(); InstrumentSetupModel restored; restored.loadXml(r);
                require(!r.hasError(), "invalid draft XML resave rejected");
                bool found = false;
                for (const auto& item : restored.getConfig().instruments) if (item.name == "Relay1") {
                    found = true;
                    require(item.commConfig.toResourceString() == resource && item.commConfig.timeout == 1234,
                            "invalid draft XML resave lost values");
                }
                require(found, "invalid draft XML resave lost Relay1");
            }
        }
        require(currentRelay().getResourceString() == "MODBUS::COM42:14400:8:E:2::SLAVE:17" &&
                currentRelay().commConfig.timeout == 1234, "corrected draft did not commit");
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
        else if(mode=="serial") { serialSettingsTest(); slaveIdXmlTest(); }
        else if(mode=="tcp") tcpSettingsTest();
        else if(mode=="ac_catalog") acCatalogTest();
        else if(mode=="load_frames") loadFrameChoicesTest();
        else if(mode=="instrument_setup_serial") { instrumentSetupSerialTest(); invalidRtuDraftTest(); }
        else throw std::runtime_error("unknown scenario");
        require(QThreadPool::globalInstance()->waitForDone(5000),"worker leaked");
        return 0;
    } catch(const std::exception &error) { std::cerr<<error.what(); return 1; }
}
