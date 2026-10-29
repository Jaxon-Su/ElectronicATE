#include "testrunservice.h"
#include "testsequenceconditionmapping.h"
#include "testsequencetaskdefaults.h"
#include "testsequencemodel.h"
#include "testsequencesettingsvalidation.h"
#include "testsequenceexcelreport.h"
#include "taskresultformatter.h"
#include "oscilloscope.h"
#include "transientstrategy.h"
#include "binaryfilestore.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }

TestSequenceExecutionContext context()
{
    TestSequenceExecutionContext c;
    c.inputs.append({"1phase", "90", "47", "0"});
    c.dcInputs.append(DcGroup{});
    c.dcInputs[0][0].vin = "48";
    c.dcLabels.append("DC input");
    c.dynamics = {{"A <&>", {"0~1"}}, {"", {}}, {"B", {"1~2"}}, {"C", {"2~3"}}};
    c.dynamicMeta.t1t2 = {"1~2", "", "2~3", "3~4"};
    return c;
}
QVariantMap config()
{
    return {{"ac_input_index", 0}, {"dyload_indices", QVariantList{3, 0, 2}},
            {"delay_ms", 0}, {"power_off_ms", 0}, {"startup_ms", 0}, {"auto_period", true}, {"measure_target", "BOTH"}};
}

void mappingAndXml()
{
    auto c = context();
    auto cfg = config();
    TestSequenceConditions::capture(cfg, c);
    QString error;
    require(TestSequenceConditions::resolve(cfg, c, error), qPrintable(error));
    require(cfg["dyload_indices"].toList() == QVariantList({0, 2, 3}), "group order not TestConditions order");
    std::swap(c.dynamics[0], c.dynamics[3]);
    std::swap(c.dynamicMeta.t1t2[0], c.dynamicMeta.t1t2[3]);
    require(TestSequenceConditions::resolve(cfg, c, error), "reordering lost condition references");
    require(cfg["dyload_labels"].toStringList() == QStringList({"C", "B", "A <&>"}), "labels did not follow remap");
    auto changed = c;
    changed.dynamicMeta.t1t2[2] = "100~200";
    auto stale = cfg;
    require(!TestSequenceConditions::resolve(stale, changed, error), "changed timing silently accepted");
    TestSequenceConditions::prepareDialog(stale, changed);
    require(stale["dyload_indices"].toList().isEmpty(), "stale selection remained checked");
    changed = c;
    changed.dynamics.append(c.dynamics[0]);
    changed.dynamicMeta.t1t2.append(c.dynamicMeta.t1t2[0]);
    stale = cfg;
    require(!TestSequenceConditions::resolve(stale, changed, error), "ambiguous condition accepted");
    changed = c;
    changed.dynamics.removeAt(2); changed.dynamicMeta.t1t2.removeAt(2);
    stale = cfg;
    require(!TestSequenceConditions::resolve(stale, changed, error), "deleted condition accepted");

    cfg["capture_enabled"] = true;
    cfg["capture_settings"] = QVariantMap{{"formats", QStringList{"WFM", "PNG"}}, {"channel", 0}};
    TestSequenceModel model;
    DutRowData row; row.item = "Dynamic Group Test"; row.retry = "2";
    row.settings = {{"dynamicGroup", cfg}}; model.appendTask(row);
    QString xml; QXmlStreamWriter out(&xml); model.writeXml(out);
    require(!out.hasError(), "XML save failed");
    TestSequenceModel loaded; QXmlStreamReader in(xml); in.readNextStartElement(); loaded.loadXml(in);
    require(!in.hasError() && loaded.tasks().size() == 1, "XML load failed");
    require(QJsonDocument::fromVariant(loaded.tasks()[0].settings) == QJsonDocument::fromVariant(row.settings), "XML group settings changed");
    auto rejectXml = [&](const QVariantMap &invalid) {
        QString text;
        QXmlStreamWriter xmlOut(&text);
        xmlOut.writeStartElement("TestSequence"); xmlOut.writeStartElement("DutTable"); xmlOut.writeStartElement("Row");
        xmlOut.writeAttribute("Active", "1"); xmlOut.writeAttribute("Report", "1");
        xmlOut.writeTextElement("Item", "Dynamic Group Test"); xmlOut.writeTextElement("Retry", "0");
        xmlOut.writeTextElement("Settings", QString::fromUtf8(QJsonDocument::fromVariant(
            QVariantMap{{"dynamicGroup", invalid}}).toJson(QJsonDocument::Compact)));
        xmlOut.writeEndElement(); xmlOut.writeEndElement(); xmlOut.writeEndElement();
        int notifications = 0;
        auto connection = QObject::connect(&loaded, &TestSequenceModel::configLoaded, [&] { ++notifications; });
        QXmlStreamReader bad(text); bad.readNextStartElement(); loaded.loadXml(bad);
        QObject::disconnect(connection);
        require(bad.hasError() && notifications == 0 && loaded.tasks().size() == 1 &&
                    QJsonDocument::fromVariant(loaded.tasks()[0].settings) == QJsonDocument::fromVariant(row.settings),
                "invalid group XML changed live data");
    };
    for (const auto &bad : {QVariant("0,2"), QVariant(QVariantList{0, 0}), QVariant(QVariantList{-1}),
                           QVariant(QVariantList{0.5}), QVariant(QVariantList{true}), QVariant(QVariantList{"0"})}) {
        auto invalid = cfg; invalid["dyload_indices"] = bad;
        rejectXml(invalid);
        require(!TestSequenceSettingsValidation::config("dynamicGroup", QJsonObject::fromVariantMap(invalid), error), "invalid XML selection accepted");
        bool rejected = false;
        try { TaskSettings::Group decoded(TestSequenceTaskKind::Kind::DynamicGroup, invalid); } catch (...) { rejected = true; }
        require(rejected, "invalid execution selection accepted");
    }
    auto mismatch = cfg;
    mismatch["dyload_labels"] = QStringList{"one"}; rejectXml(mismatch);
    mismatch = cfg; mismatch["condition_refs"] = QVariantMap{{"dyload", QVariantList{"one"}}}; rejectXml(mismatch);
    auto empty = testSequenceDefaultSettings("Dynamic Group Test");
    require(TestSequenceSettingsValidation::settings("Dynamic Group Test", QJsonObject::fromVariantMap(empty), error), "empty draft rejected");
    TaskPayload draft; draft.task.name = "Dynamic Group Test"; draft.settings = empty["dynamicGroup"].toMap();
    require(!TestSequenceConditions::validate(draft, c, error), "empty draft executable");
}

struct State {
    QString mode, direction = "BOTH";
    int current = -1, inputs = 0, periods = 0, measures = 0, starts = 0, screenshots = 0, waveforms = 0;
    bool failed = false;
    QVector<int> loads, off;
    QStringList trace;
};
class Scope : public Oscilloscope {
public:
    int source = 1;
    explicit Scope(State &state) : s(state) {}
    QString vendor() const override { return "Offline"; }
    QString model() const override { return "MSOSeries456"; }
    bool isConnected() const override { return true; }
    int getTotalChannel() override { return 2; }
    bool isChannelEnabled(int) override { return true; }
    bool selectEdgeTrigger(QString&) override { s.trace << "edge"; return true; }
    bool setEdgeTriggerChannel(int channel, QString& error) override {
        if (channel < 1 || channel > getTotalChannel()) { error = "invalid trigger source"; return false; }
        source = channel; s.trace << QString("source CH%1").arg(channel); return true;
    }
    int triggerChannel() override { return source; }
    bool prepareSteadyAcquisition(QAtomicInt&, QString&, bool startContinuous) override {
        require(startContinuous != s.mode.startsWith("capture"), "Capture preparation started continuous acquisition");
        require(s.trace.last() == "settled", "startup before settle");
        ++s.starts; s.trace << "startup"; return true;
    }
    bool stopAcquisition(QString &error) override {
        s.trace << "stop";
        if (s.mode == "cleanup") { error = "stop failed"; return false; }
        return true;
    }
    ScopeCaptureSource captureSource() override { return {"CH1", 1}; }
    QString captureSlope() override { return "RISE"; }
    ScopeAcquisitionState acquisitionState() override { return ScopeAcquisitionState::Completed; }
    double getTriggerLevel() override { return 10 + s.current; }
    double getTimebase() override { return .001; }
    double getChannelScale(int) override { return 100; }
    double getChannelPosition(int) override { return 0; }
    bool readChannelOffset(int, double &value, QString&) override { value = 0; return true; }
    bool waitForOperationComplete(int) override { return true; }
    QByteArray captureScreenshot(const QString&, const QString&) override { ++s.screenshots; return "image"; }
    bool captureWaveformFileToHost(int, const QString &path, const QString&, const QString&, int, int) override {
        ++s.waveforms;
        if (s.mode == "capture_failure" && s.current == 2) return false;
        return BinaryFileStore::save(path, QByteArray::number(s.current)).succeeded();
    }
    State &s;
};
class Strategy : public IOscilloscopeMeasureStrategy {
public:
    explicit Strategy(State &state) : s(state) {}
    QString name() const override { return "offline dynamic"; }
    OscMeasureResult execute(IScopeMeasurement*, QAtomicInt &stop) override {
        require(s.trace.last() == (s.mode == "no_period" ? "startup" : "period"), "search before acquisition preparation");
        ++s.measures; s.trace << "measure";
        OscMeasureResult result;
        result.success = true;
        if (s.current == 2 && (s.mode == "failure" || (s.mode == "retry" && !s.failed))) {
            s.failed = true; result.errorMessage = "search failure"; result.success = false; return result;
        }
        const double max = s.current == 0 ? (s.mode == "retry" && !s.failed ? 900 : 100) : s.current == 2 ? 80 : 90;
        result.channels = {{1, max, s.current == 2 ? -50.0 : -10.0, 10.0 + s.current, 20.0 + s.current},
                           {2, 50.0 + s.current, -9.0 + s.current, 30.0 + s.current, 40.0 + s.current}};
        if (m_captureObserver) {
            m_captureObserver({result.channels});
            m_captureObserver({result.channels}); // Another completed acquisition with identical values must also be archived.
        }
        for (auto direction : {MeasurementDirection::Rise, MeasurementDirection::Fall}) {
            if ((s.direction == "MAX" && direction == MeasurementDirection::Fall) ||
                (s.direction == "MIN" && direction == MeasurementDirection::Rise)) continue;
            MeasurementTrial trial{int(result.trials.size())+1, direction, 1, 1, 0, {}};
            for (const auto &v : result.channels) trial.channels.append({v.channel, {}, v.maxVal, v.minVal, v.rms, v.mean});
            result.trials.append(trial);
        }
        for (auto &channel : result.channels) {
            if (s.direction == "MAX") channel.minVal = std::numeric_limits<double>::quiet_NaN();
            if (s.direction == "MIN") channel.maxVal = std::numeric_limits<double>::quiet_NaN();
        }
        if (s.mode == "cancel" && s.current == 2) stop.storeRelease(1);
        return result;
    }
    State &s;
};

void runCase(const QString &mode)
{
    State s; s.mode = mode;
    if (mode == "rise") s.direction = "MAX";
    if (mode == "fall") s.direction = "MIN";
    Scope scope(s);
    TestRunDependencies deps; deps.validateMeasurement = [](const auto &, auto, int, bool) {};
    deps.instruments.input = [&](const InstrumentSetupConfig&, const InputRow&, InputAction action) {
        ++s.inputs; s.trace << (action == InputAction::PowerOn ? "input on" : "input off");
        if (s.mode == "input_failure" && action == InputAction::PowerOn) return InstrumentOperationResult{false, "input failed"};
        return InstrumentOperationResult{};
    };
    deps.instruments.dcGroup = [&](const InstrumentSetupConfig&, const DcGroup &rows, InputAction action) {
        require(rows[0].vin == "48", "DC snapshot changed");
        ++s.inputs; s.trace << (action == InputAction::PowerOn ? "input on" : "input off"); return InstrumentOperationResult{};
    };
    deps.instruments.dynamic = [&](const InstrumentSetupConfig&, const QVector<DynamicDataRow>&, int index,
                                   const DynamicMetaRow&, DynamicLoadAction action, bool, bool) {
        if (action == DynamicLoadAction::LoadOn) { s.current = index; s.loads << index; s.trace << "load on"; }
        else if (action == DynamicLoadAction::LoadOff) { s.off << index; s.trace << "load off"; }
        else s.trace << "load configure";
        if (s.mode == "load_failure" && action == DynamicLoadAction::LoadOn) return InstrumentOperationResult{false, "load failed"};
        return InstrumentOperationResult{};
    };
    deps.instruments.relay = [&](const InstrumentSetupConfig&, const QVector<RelayDataRow>&, int, RelayAction) {
        throw std::runtime_error("preflight allowed earlier relay operation"); return InstrumentOperationResult{};
    };
    deps.strategy = makeMeasurementStrategyFactory([&](const QString &name, const TransientContext *cfg) {
        require(name == "Dynamic Test" && cfg && cfg->target == s.direction, "group did not use single Dynamic strategy/direction");
        return std::make_unique<Strategy>(s);
    });
    deps.autoPeriod = [&](IScopeAutoPeriod*, QAtomicInt&, QString &error, const OscCaptureObserver&, const VerticalScaleOptions&,const std::function<void(const QString&)>&) {
        require(s.trace.last() == "startup", "period before new acquisition startup");
        ++s.periods; s.trace << "period";
        if (s.mode == "period_failure" && s.current == 2) { error = "period failure"; return false; }
        return true;
    };
    TestRunService service(deps);
    auto c = context(); c.scope = &scope;
    QTemporaryDir dir; c.reportDirectory = dir.path();
    TaskPayload task; task.task.name = "Dynamic Group Test"; task.settings = config(); task.settings["measure_target"] = s.direction;
    if (mode == "dc") {task.settings["ac_input_index"] = -1; task.settings["dc_input_index"] = 0;}
    if (mode == "single") task.settings["dyload_indices"] = QVariantList{2};
    if (mode == "no_period") task.settings["auto_period"] = false;
    if (mode == "retry" || mode == "cancel" || mode == "cleanup") task.retry = 1;
    if (mode == "capture" || mode == "capture_failure") {
        task.settings["capture_enabled"] = true;
        task.settings["capture_settings"] = QVariantMap{{"formats", QStringList{"WFM", "PNG"}}, {"channel", 1}};
    }
    QVector<TaskPayload> tasks{task};
    if (mode == "preflight") {
        tasks[0].settings["dyload_indices"] = QVariantList{0, 99};
        c.relays = {{"off", {"OFF"}}}; // Reject the later group, not an invalid earlier Relay row.
        TaskPayload relay; relay.task.name = "Relay"; relay.settings["relay_index"] = 0; tasks.prepend(relay);
    }
    QVector<TestSequenceResultRecord> records;
    QVector<TestSequenceResultRecord> progress;
    bool finished = false;
    QObject::connect(&service, &TestRunService::resultRecordsReady, [&](const auto &values) { records = values; });
    QObject::connect(&service, &TestRunService::measurementReady, [&](int, const auto &record, bool) { progress.append(record); });
    QObject::connect(&service, &TestRunService::finished, [&] { finished = true; });
    QObject::connect(&service, &TestRunService::logMessage, [&](const QString &text) { if (text.contains(": settling for")) s.trace << "settled"; });
    service.prepareRun(); service.startTasks(tasks, c);
    require(finished && !records.isEmpty(), "group did not finish");
    if (mode == "preflight") {
        require(records.size() == 2 && records[0].status == "Skipped" && records[1].attempt == 0 && s.inputs == 0, "batch preflight side effect"); return;
    }
    const auto &record = records.last();
    const bool success = !QStringList{"failure", "period_failure", "capture_failure", "input_failure", "load_failure", "cancel", "cleanup"}.contains(mode);
    require((record.status == "Pass") == success, "group pass/fail mismatch");
    require(record.conditions.size() == (mode == "single" ? 1 : 3), "condition audit missing");
    require(s.periods == (mode == "no_period" ? 0 : s.starts), "Auto Period not per condition");
    for (int index : s.loads) require(s.off.contains(index), "attempted load never cleaned up");
    if (mode == "input_failure" || mode == "load_failure") {
        require(s.measures == 0 && s.off == QVector<int>({0,0}) && s.trace.contains("input off") &&
                record.conditions[0].status == "Fail" && record.conditions[1].status == "Skipped",
                "output failure continued measurement or omitted cleanup");
    }
    if (mode == "retry") {
        require(records.size() == 2 && s.loads == QVector<int>({0, 2, 0, 2, 3}), "retry did not restart entire group");
        require(records.first().conditions.first().channels.first().maximum == 900 &&
                record.conditions.first().channels.first().maximum == 100, "attempt results mixed");
    }
    if (mode == "cancel" || mode == "failure" || mode == "period_failure" || mode == "capture_failure") {
        require(s.loads == QVector<int>({0, 2}) && record.conditions.last().status == "Skipped", "continued after group failure/stop");
        require(record.status == (mode == "cancel" ? "Cancelled" : "Fail"), "cancel/fail status lost");
    }
    require(record.channels.isEmpty(), "steady Group mixed values across conditions");
    if (success) {
        for (const auto &condition : record.conditions) {
            const int index = condition.condition.index;
            require(condition.channels.size() == (s.direction == "BOTH" ? 4 : 2), "direction/channel representatives missing");
            const auto &one = condition.channels[0];
            const double expectedMax = index == 0 ? 100 : index == 2 ? 80 : 90;
            require(one.maximum == expectedMax, "per-load Max changed");
            require(one.minimum == (index == 2 ? -50 : -10), "per-load Min changed");
            require(one.rms == 10 + index && one.mean == 20 + index, "per-load RMS/Mean overwritten");
            require(condition.channels[1].rms == 30 + index && condition.channels[1].mean == 40 + index,
                    "per-channel supplementary values mixed");
            const auto text = TaskResultFormatter::measurement(record, true);
            require(text.contains("View details") && !text.contains("RMS"), "Tasks summary expanded values");
        }
    }
    if (success && mode != "single") {
        require(progress.first().conditions[1].status == "Skipped", "later writes mutated earlier progress snapshots");
        require(TaskResultFormatter::measurement(record, true).contains("3 loads"), "Tasks summary omitted load count");
    }
    if (mode == "capture") {
        require(s.screenshots == 6 && s.waveforms == 6, "not every completed group record saved");
        require(record.files.size() == 12, "group completed record files missing");
        for (const auto &path : record.files) {
            require(path.contains("_DyLoad"), "capture filename lacks condition");
            require(path.endsWith(".png") || path.endsWith(".wfm"),"unselected format saved");
            if (path.endsWith(".wfm")) {
                QFile f(path); require(f.open(QIODevice::ReadOnly),"waveform missing");
                const int row = f.readAll().toInt()+1;
                require(row == 1 || row == 3 || row == 4,"wrong capture condition");
                require(path.contains(QString("_DyLoad%1_").arg(row)),"waveform mixed across conditions");
            }
        }
    }
    if (mode == "capture_failure") require(record.files.size() == 4, "failed candidate leaked files or removed old winner");
    QString path, error;
    require(TestSequenceExcelReport::save(dir.path(), "group", records, path, error), qPrintable(error));
    QFile excel(path); require(excel.open(QIODevice::ReadOnly), "Excel missing");
    const auto bytes = excel.readAll();
    require(bytes.contains("Dynamic Group Test") && bytes.contains("Dy Load row"), "Excel omitted group/source details");
    require(!bytes.contains("Group summary") && !bytes.contains("RMS/Mean last record"), "Excel retained mixed Group summary");
    if (success && mode != "single") require(bytes.count(">Pass<") == (mode == "retry" ? 32 : s.direction == "BOTH" ? 24 : 15), "Excel lost direction/Trial rows");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QString mode = argc > 1 ? argv[1] : "success";
        if (mode == "xml") mappingAndXml(); else runCase(mode);
        std::cout << "PASS dynamic group " << mode.toStdString() << '\n';
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
