#include "testrunservice.h"
#include "oscilloscope.h"
#include "oscilloscopeconfiguration.h"
#include "testsequencetaskdefaults.h"
#include "testsequenceconditionmapping.h"
#include "testsequencesettingsvalidation.h"
#include "../../../src/service/instrumentexecutor/measurementpreflight.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
class Scope final : public Oscilloscope {
public:
    int count = 4, io = 0; bool enabled = false;
    QString model() const override { return "MSOSeries456"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString &, const QString &) override { ++io; return {}; }
    int getTotalChannel() override { return count; }
    bool isChannelEnabled(int) override { ++io; return enabled; }
    bool stopAcquisition(QString &) override { ++io; return true; }
    bool applySettings(const OscilloscopeSettings &settings, QAtomicInt &, QString &,
                       OscilloscopeConfigurationResult *) override {
        ++io;
        for (const auto &channel : settings.channels) if (channel.index == count) enabled = channel.enabled;
        return true;
    }
};
class Strategy final : public IMeasurementStrategy {
public:
    explicit Strategy(Scope &scope, bool needsEnabled, std::function<void()> cleanup)
        : scope(scope), needsEnabled(needsEnabled), cleanup(std::move(cleanup)) {}
    QString name() const override { return "preflight control"; }
    MeasurementResult execute(QAtomicInt &) override {
        require(!needsEnabled || scope.enabled, "earlier Write Scope did not enable trigger");
        if (cleanup) cleanup();
        MeasurementResult result; result.success = true; return result;
    }
private:
    Scope &scope; bool needsEnabled; std::function<void()> cleanup;
};
InstrumentConfig instrument(QString name, QString type, QString model, int address) {
    InstrumentConfig value; value.name = name; value.type = type; value.modelName = model;
    value.commConfig.protocol = ProtocolType::GPIB; value.commConfig.gpibAddress = address; return value;
}
void run(const QString &name, int count, int requested, const QString &mode) {
    Scope scope; scope.count = count; QTemporaryDir directory;
    TestSequenceExecutionContext context; context.scope = &scope; context.reportDirectory = directory.path();
    auto ac = instrument("ACSource", "InputSource", "61505", 1);
    auto load = instrument("Load1", "Load", "63600-5", 3);
    load.channels = {{"63640-80-80", 1, 0}}; load.channelNumbers = {1};
    auto relay = instrument("Relay1", "Relay", "Modbus_RTU_4CH", 4);
    relay.channels = {{"relay", 1, 0}, {"relay", 2, 0}}; relay.channelNumbers = {1, 2};
    context.instrumentSetup.instruments = {ac, load, relay}; context.instrumentSetup.relayOutputs = 2;
    context.inputs = {{"1phase", "120", "60", "0"}}; context.loads = {{"Load", {"1"}}};
    context.loadMeta.modes = {"CC"}; context.loadMeta.ranges = {"Auto Range"};
    context.loadMeta.vo = {"12"}; context.loadMeta.von = {"0"};
    context.relays = {{"Short", {"ON", "OFF"}}, {"Discharge", {"OFF", "ON"}}};
    TaskPayload first; first.task.name = "Relay"; first.settings = {{"relay_index", 0}};
    TaskPayload task; task.task.name = name;
    task.settings = testSequenceDefaultSettings(name).value(TestSequenceTaskKind::settingsGroup(name)).toMap();
    task.settings["ac_input_index"] = 0; task.settings["load_index"] = 0; task.settings["delay_ms"] = 0;
    task.settings["trigger_channel"] = requested;
    if (name.contains("descent")) task.settings["trigger_levels"] = QVariantList{10.0};
    if (name.contains("short")) { task.settings["relay_index"] = 0; task.settings["discharge_index"] = 1; }
    const auto writeConfig = [&](int channel, bool enabled) {
        return QVariantMap{{"ignore_horizontal", true}, {"ignore_acquire", true}, {"ignore_trigger", true},
                           {"ignore_channels", mode == "ignore"}, {QString("ch%1_enabled").arg(channel), enabled}};
    };
    if (mode == "enable") { first.task.name = "Write Oscilloscope"; first.settings = writeConfig(count, true); }
    if (name == "Write Oscilloscope") task.settings = writeConfig(requested, mode != "disabled");
    if (mode == "log") {
        task.settings["mode"] = "TRACK"; task.settings["capture_enabled"] = true;
        task.settings["capture_settings"] = QVariantMap{{"directory", directory.path()}, {"formats", QStringList{"PNG"}}};
    }
    for (auto *payload : {&first, &task}) {
        TestSequenceConditions::capture(payload->settings, context);
        QString error; require(TestSequenceConditions::validate(*payload, context, error), qPrintable(error));
    }
    int outputs = 0, creations = 0; TestRunDependencies deps;
    deps.validateMeasurement = validateMeasurementConditions;
    deps.prepareConfiguration = [](const auto &model, int channels, const auto &cfg, auto &out, auto &error) {
        return prepareOscilloscopeSettings(cfg, model, channels, out, error);
    };
    deps.prepareBrownOut = [](const auto &, const auto &) -> BrownOutIoFactory {
        return []() -> std::unique_ptr<IBrownOutIo> { throw std::runtime_error("fake strategy must not open Brown IO"); };
    };
    deps.instruments.input = [&](const auto &, const auto &, InputAction) { ++outputs; return InstrumentOperationResult{}; };
    deps.instruments.load = [&](const auto &, const auto &, int, const auto &, LoadAction, bool) { ++outputs; return InstrumentOperationResult{}; };
    deps.instruments.relay = [&](const auto &, const auto &, int, RelayAction) { ++outputs; return InstrumentOperationResult{}; };
    deps.strategy = [&](const MeasurementStrategyRequest &request) -> std::unique_ptr<IMeasurementStrategy> {
        std::function<void()> cleanup;
        if (const auto *measurement = std::get_if<ScopeMeasurementRequest>(&request)) {
            cleanup = [context = measurement->context] {
                QString error;
                require(context.power(false, error), "fake input OFF failed");
                require(context.relays(false, false, error), "fake relay release failed");
                require(context.load(false, error), "fake load OFF failed");
            };
        }
        ++creations; return std::make_unique<Strategy>(scope, mode == "enable", std::move(cleanup));
    };
    TestRunService service(deps); QVector<TestSequenceResultRecord> records; QStringList logs;
    QObject::connect(&service, &TestRunService::resultRecordsReady, [&](const auto &value) { records = value; });
    QObject::connect(&service, &TestRunService::logMessage, [&](const auto &value) { logs << value; });
    service.startTasks({first, task}, context);
    require(records.size() == 2, "two results expected");
    const bool invalid = requested > count && mode != "ignore" && mode != "disabled";
    std::cout << name.toStdString() << " " << mode.toStdString() << " count=" << count
              << " CH" << requested << " " << records[1].status.toStdString() << " " << records[1].error.toStdString() << '\n';
    if (invalid) {
        require(outputs == 0 && scope.io == 0 && creations == 0, "invalid later channel allowed IO");
        require(records[0].status == "Skipped" && records[1].status == "Fail" && records[0].attempt == 0 && records[1].attempt == 0, "not whole-batch preflight");
        require(records[1].error.contains(QString("CH%1").arg(requested)), "missing requested channel diagnostic");
    } else {
        require(records[0].status == "Pass" && records[1].status == "Pass", qPrintable(logs.join('\n')));
        if (name != "Write Oscilloscope") require(creations == 1, "valid measurement never dispatched");
    }
    if (mode == "log") {
        const QString log = logs.join('\n');
        require(log.contains("pause voltage descent") && log.contains("READY") && log.contains("resume"), "Track Capture pause/resume log missing");
        require(!log.contains("voltage descent continues"), "obsolete Brown Out log");
    }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const QString mode = argc > 1 ? argv[1] : "trigger";
        if (mode == "trigger") for (const QString name : {"Brown Out", "Turn off (descent)", "Turn on then short (descent)"}) {
            for (int count : {4, 6}) run(name, count, 8, "reject");
            for (int count : {4, 6, 8}) run(name, count, count, "enable");
        } else if (mode == "write") {
            for (int count : {4, 6}) run("Write Oscilloscope", count, 8, "reject");
            for (int count : {4, 6, 8}) for (const QString control : {"ignore", "disabled", "normal"})
                run("Write Oscilloscope", count, control == "normal" ? count : 8, control);
        } else run("Brown Out", 4, 4, "log");
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
