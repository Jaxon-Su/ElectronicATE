#define main dynamic_group_existing_main
#include "dynamic_group_test.cpp"
#undef main

struct GroupState : State {
    bool armed = false, event = false;
    double level = 0;
    ScopeTriggerMode triggerMode = ScopeTriggerMode::Normal;
    QString base;
    bool inputOn = false, dcOn = false, loadOn = false;
    QVector<QString> relayMask{"OFF", "OFF"};
};
class GroupScope : public Scope {
public:
    explicit GroupScope(GroupState &state) : Scope(state), g(state) {}
    int triggerChannel() override { return 1; }
    bool isClipping(int) override { return false; }
    bool isChannelEnabled(int channel) override { return channel == 1; }
    bool selectEdgeTrigger(QString&) override { g.trace << "edge"; return true; }
    bool setEdgeSlope(ScopeTriggerSlope value, QString&) override { g.trace << QString("slope %1").arg(int(value)); return true; }
    bool setTriggerMode(ScopeTriggerMode value, QString&) override { g.triggerMode = value; g.trace << QString("mode %1").arg(int(value)); return true; }
    bool setChannelTriggerLevel(int channel, double value, QString&) override {
        require(channel == 1, "wrong trigger channel"); g.level = value;
        g.trace << QString("level %1").arg(value); return true;
    }
    double getChannelScale(int) override { return 1; }
    double getTriggerLevel() override { return g.level; }
    bool beginSingleAcquisition() override { g.armed = true; g.event = false; g.trace << "arm"; return true; }
    ScopeAcquisitionState acquisitionState() override {
        if (g.base == "Static Test" || g.base == "Dynamic Test") return ScopeAcquisitionState::Completed;
        return g.triggerMode == ScopeTriggerMode::Auto ||
                       (g.mode != "no_event" && g.event && g.level >= -3 && g.level <= 4)
            ? ScopeAcquisitionState::Completed : ScopeAcquisitionState::Running;
    }
    ScopeTriggerState triggerState() override { return ScopeTriggerState::Ready; }
    double readMeasurement(int, ScopeMeasurement metric) override {
        require(!TestSequenceTaskKind::isTransient(TestSequenceTaskKind::fromName(g.base)) ||
                    (g.event && g.triggerMode != ScopeTriggerMode::Auto) ||
                    ((g.base == "Turn off" || g.base == "Turn on then short") && g.triggerMode == ScopeTriggerMode::Auto) ||
                    (metric != ScopeMeasurement::Rms && metric != ScopeMeasurement::Mean),
                "transient reference/Baseline queried RMS/Mean");
        require(acquisitionState() == ScopeAcquisitionState::Completed, "stale waveform read");
        switch (metric) {
        case ScopeMeasurement::Maximum: return g.current == 0 ? 4 : 3;
        case ScopeMeasurement::Minimum: return g.current == 0 ? -1 : -3;
        case ScopeMeasurement::Rms: return 1 + g.current;
        case ScopeMeasurement::Mean: return .5 + g.current;
        }
        return NAN;
    }
    bool stopAcquisition(QString &error) override { g.armed = false; return Scope::stopAcquisition(error); }
    GroupState &g;
};
class GroupStrategy : public IOscilloscopeMeasureStrategy {
public:
    GroupStrategy(GroupState &state, TransientContext context) : g(state), c(std::move(context)) {}
    QString name() const override { return g.base; }
    OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stop) override {
        ++g.measures;
        OscMeasureResult result;
        if (g.base == "Static Test" || g.base == "Dynamic Test") {
            result.success = true;
            result.channels = {{1, g.current == 0 ? 4.0 : 3.0, g.current == 0 ? -1.0 : -3.0,
                                1.0 + g.current, .5 + g.current}};
            if (m_captureObserver) m_captureObserver({result.channels});
            const auto &v = result.channels.front();
            for (auto direction : {MeasurementDirection::Rise, MeasurementDirection::Fall})
                result.trials.append({int(result.trials.size())+1, direction, 1, 1, 0,
                    {{v.channel, {}, v.maxVal, v.minVal, v.rms, v.mean}}});
        } else {
            c.timeoutMs = 3; c.phaseTimeoutMs = 10000;
            c.settleMs = c.powerOffMs = c.dischargeMs = 0; c.trialsPerLevel = 1;
            const auto kind = g.base == "Turn on" ? TransientKind::TurnOn : g.base == "Turn off" ? TransientKind::TurnOff
                : g.base == "Short then turn on" ? TransientKind::ShortThenTurnOn : TransientKind::TurnOnThenShort;
            c.captureRecord = m_captureObserver;
            result = runTransientStrategy(kind, c, scope, stop);
        }
        if (g.current == 2) {
            if (g.mode == "failure" || ((g.mode == "retry" || g.mode == "capture_retry") && !g.failed)) {
                g.failed = true; result.success = false; result.errorMessage = "injected member failure";
            }
            if (g.mode == "cancel") stop.storeRelease(1);
            if (g.mode == "cleanup_contract") { result.cleanupFailed = true; result.errorMessage = "cleanup failed"; }
        }
        return result;
    }
    GroupState &g;
    TransientContext c;
};

TestSequenceExecutionContext groupContext()
{
    auto c = context();
    c.loads = {{"L1", {"1"}}, {"", {}}, {"L3", {"3"}}, {"L4", {"4"}}};
    c.relays = {{"Short", {"ON", "OFF"}}, {"Discharge", {"OFF", "ON"}}};
    c.instrumentSetup.relayOutputs = 2;
    InstrumentConfig relay;
    relay.type = "Relay"; relay.enabled = true;
    relay.commConfig = CommunicationConfig::fromResourceString("MODBUS::COM1:9600:8:N:1::SLAVE:1");
    require(relay.commConfig.isValid() && relay.commConfig.protocol == ProtocolType::Modbus_RTU,
            "group relay fixture must use a valid Modbus RTU address");
    relay.channels.resize(2); relay.channels[0].index = 1; relay.channels[1].index = 2;
    c.instrumentSetup.instruments.append(relay);
    return c;
}
QVariantMap groupConfig(TestSequenceTaskKind::Kind kind)
{
    const auto key = TestSequenceTaskKind::loadKey(kind);
    auto cfg = testSequenceDefaultSettings(TestSequenceTaskKind::name(kind)).first().toMap();
    cfg["ac_input_index"] = 0; cfg[key + "_indices"] = QVariantList{2, 0};
    cfg["delay_ms"] = 0;
    if (TestSequenceTaskKind::isSteady(kind)) {
        cfg["auto_period"] = true;
        cfg["power_off_ms"] = cfg["startup_ms"] = 0;
    }
    if (cfg.contains("discharge_index")) cfg["discharge_index"] = 1;
    if (cfg.contains("relay_index")) cfg["relay_index"] = 0;
    return cfg;
}

QVector<TestSequenceResultRecord> executeGroup(TestSequenceTaskKind::Kind kind, const QString &mode, bool singles,
                                      GroupState &g, const QString &directory)
{
    g.base = TestSequenceTaskKind::name(TestSequenceTaskKind::singleKind(kind)); g.mode = mode;
    GroupScope scope(g); auto ctx = groupContext(); ctx.scope = &scope; ctx.reportDirectory = directory;
    const bool dual = mode.startsWith("dual");
    std::function<void()> cancel;
    TestRunDependencies d; d.validateMeasurement = [](const auto &, auto, int, bool) {};
    d.instruments.input = [&](const InstrumentSetupConfig&, const InputRow&, InputAction action) {
        g.inputOn = action == InputAction::PowerOn;
        g.trace << (g.inputOn ? "input on" : "input off");
        if (g.armed) g.event = true;
        if (g.inputOn && mode == "dual_cancel") cancel();
        if (!g.inputOn && mode == "dual_off_exception") throw std::runtime_error("AC off exception");
        return InstrumentOperationResult{};
    };
    d.instruments.dcGroup = [&](const InstrumentSetupConfig&, const DcGroup &rows, InputAction action) {
        require(rows[0].vin == "48", "DC group snapshot lost");
        g.dcOn = action == InputAction::PowerOn;
        g.trace << (g.dcOn ? "dc on" : "dc off");
        if (g.dcOn && mode == "dual_on_failure") return InstrumentOperationResult{false, "DC on failure"};
        if (g.dcOn && mode == "dual_on_exception") throw std::runtime_error("DC on exception");
        return InstrumentOperationResult{};
    };
    if (mode == "dual_missing") d.instruments.dcGroup = {};
    auto load = [&](int index, bool on) {
        g.loadOn = on;
        if (on) { g.current = index; g.loads << index; } else g.off << index;
        g.trace << QString("load %1 %2").arg(index).arg(on);
        return InstrumentOperationResult{};
    };
    d.instruments.load = [&](const InstrumentSetupConfig&, const QVector<LoadDataRow>&, int index, const LoadMetaRow&, LoadAction action, bool) {
        return load(index, action == LoadAction::LoadOn);
    };
    d.instruments.dynamic = [&](const InstrumentSetupConfig&, const QVector<DynamicDataRow>&, int index, const DynamicMetaRow&, DynamicLoadAction action, bool, bool) {
        return load(index, action == DynamicLoadAction::LoadOn);
    };
    d.instruments.relay = [&](const InstrumentSetupConfig&, const QVector<RelayDataRow> &rows, int index, RelayAction) {
        g.relayMask = rows[index].values;
        g.trace << "relays " + QStringList(g.relayMask.begin(), g.relayMask.end()).join(',');
        if (g.armed && g.relayMask[0] == "ON") g.event = true;
        return InstrumentOperationResult{};
    };
    d.strategy = makeMeasurementStrategyFactory([&](const QString &name, const TransientContext *c) {
        require(name == g.base && c, "group used different strategy or missing context");
        if (TestSequenceTaskKind::isTransient(kind))
            require(!g.inputOn && !g.dcOn && !g.loadOn && g.relayMask == QVector<QString>({"OFF", "OFF"}), "previous member left output active");
        return std::make_unique<GroupStrategy>(g, *c);
    });
    d.autoPeriod = [&](IScopeAutoPeriod*, QAtomicInt&, QString&, const OscCaptureObserver&) { ++g.periods; return true; };
    TestRunService service(d);
    cancel = [&] { service.stop(); };
    // Scope startup is after the settle log; this also checks per-member ordering.
    QObject::connect(&service, &TestRunService::logMessage, [&](const QString &line) {
        if (line.contains("settling for")) g.trace << "settled";
    });
    QVector<TestSequenceResultRecord> results;
    QVector<TestSequenceResultRecord> progress;
    QObject::connect(&service, &TestRunService::resultRecordsReady, [&](const auto &records) { results = records; });
    QObject::connect(&service, &TestRunService::measurementReady, [&](int, const auto &record, bool) { progress.append(record); });
    TaskPayload task; task.task.name = TestSequenceTaskKind::name(kind); task.settings = groupConfig(kind);
    if (dual) task.settings["dc_input_index"] = mode == "dual_bad_index" ? 99 : 0;
    if (mode == "retry" || mode == "capture_retry" || mode == "cancel" || mode == "cleanup_contract") task.retry = 1;
    if (mode == "failure" || mode == "cancel" || mode == "cleanup_contract")
        task.settings[TestSequenceTaskKind::loadKey(kind) + "_indices"] = QVariantList{0, 2, 3};
    if (mode == "capture" || mode == "capture_retry") {
        task.settings["capture_enabled"] = true;
        task.settings["capture_settings"] = QVariantMap{{"formats", QStringList{"WFM", "PNG"}}, {"channel", 1}};
    }
    if (mode == "preflight") task.settings[TestSequenceTaskKind::loadKey(kind) + "_indices"] = QVariantList{0, 99};
    QVector<TaskPayload> tasks;
    if (singles) {
        task.task.name = g.base;
        const auto key = TestSequenceTaskKind::loadKey(kind);
        task.settings.remove(key + "_indices"); task.settings.remove(key + "_labels");
        for (int index : {0, 2}) { task.settings[key + "_index"] = index; tasks.append(task); }
    } else tasks.append(task);
    service.startTasks(tasks, ctx);
    require(!results.isEmpty(), "no results published");
    if (!singles) {
        for (const auto &record : results + progress)
            require(record.channels.isEmpty() && record.loadCondition.index == -1, "Group emitted a mixed summary/source");
    } else {
        const bool dynamic = TestSequenceTaskKind::singleKind(kind) == TestSequenceTaskKind::Kind::Dynamic;
        for (const auto &record : results + progress) {
            if (record.attempt == 0) {
                require(record.loadCondition.index == -1, "Skipped Single invented an executed load");
                continue;
            }
            const int index = record.settings.value(dynamic ? "dyload_index" : "load_index").toInt();
            require(record.loadCondition.index == index &&
                    record.loadCondition.label == (dynamic ? ctx.dynamics[index].label : ctx.loads[index].label) &&
                    record.loadCondition.kind == (dynamic ? LoadConditionKind::Dynamic : LoadConditionKind::Static),
                    "Single member lost the actual load source");
        }
    }
    require(!g.inputOn && !g.dcOn && !g.loadOn && g.relayMask == QVector<QString>({"OFF", "OFF"}), "outputs not released");
    return results;
}

void checkCaptureFolders(const QVector<TestSequenceResultRecord> &records, const QString &directory, QString strategy)
{
    strategy.replace(' ', '_');
    QSet<QString> folders;
    for (const auto &record : records) {
        require(!record.files.isEmpty(), "attempt lost its capture files");
        const QString expected = record.startedAt.toLocalTime().toString("yyyyMMdd_HHmmss")
            + QString("_Task%1_%2_Attempt%3").arg(record.taskIndex + 1, 2, 10, QChar('0'))
                  .arg(strategy).arg(record.attempt, 2, 10, QChar('0'));
        const QString folder = QDir(directory).filePath(expected);
        require(!folders.contains(folder), "different attempts shared a folder");
        folders.insert(folder);
        for (const auto &path : record.files) {
            require(QFileInfo(path).absolutePath() == folder, "service lost task, strategy, time or attempt identity");
            QFile file(path);
            require(file.open(QIODevice::ReadOnly) && !file.readAll().isEmpty(), "earlier capture deleted or emptied");
        }
        require(QDir(folder).entryList(QDir::Files).size() == record.files.size(), "files missing from attempt result");
    }
    require(QDir(directory).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == folders.size(),
            "Group created a separate folder for each load or record");
}

void checkXml(TestSequenceTaskKind::Kind kind)
{
    const auto name = TestSequenceTaskKind::name(kind), key = TestSequenceTaskKind::loadKey(kind);
    auto c = groupContext(); auto cfg = groupConfig(kind); TestSequenceConditions::capture(cfg, c);
    cfg["dc_input_index"] = 0; TestSequenceConditions::capture(cfg, c);
    QString error;
    require(TestSequenceConditions::resolve(cfg, c, error), qPrintable(error));
    TestSequenceModel original; DutRowData row; row.item = name; row.retry = "1";
    row.settings = {{TestSequenceTaskKind::settingsGroup(name), cfg}}; original.appendTask(row);
    QString xml; QXmlStreamWriter writer(&xml); original.writeXml(writer);
    TestSequenceModel restored; QXmlStreamReader reader(xml); reader.readNextStartElement(); restored.loadXml(reader);
    require(!reader.hasError() && restored.tasks().size() == 1, "group XML roundtrip failed");
    require(QJsonDocument::fromVariant(restored.tasks()[0].settings) == QJsonDocument::fromVariant(row.settings), "settings changed");
    for (const auto &bad : {QVariantList{0, 0}, QVariantList{-1}, QVariantList{true}, QVariantList{.2}, QVariantList{"0"}}) {
        auto invalid = cfg; invalid[key + "_indices"] = bad;
        require(!TestSequenceSettingsValidation::config(TestSequenceTaskKind::settingsGroup(name), QJsonObject::fromVariantMap(invalid), error), "malformed group accepted");
    }
    auto invalid = cfg; invalid[key + "_index"] = 0;
    require(!TestSequenceSettingsValidation::config(TestSequenceTaskKind::settingsGroup(name), QJsonObject::fromVariantMap(invalid), error), "mixed single/group selection accepted");
    if (TestSequenceTaskKind::isTransient(kind)) {
        invalid = cfg; invalid["auto_period"] = true;
        require(!TestSequenceSettingsValidation::config(TestSequenceTaskKind::settingsGroup(name), QJsonObject::fromVariantMap(invalid), error), "transient Auto Period accepted");
    }
    if (key == "load") {
        std::swap(c.loads[0], c.loads[2]);
        require(TestSequenceConditions::resolve(cfg, c, error) && cfg[key + "_labels"].toStringList() == QStringList({"L3", "L1"}), "static references failed reorder");
        c.loads[0].values[0] = "100";
        require(!TestSequenceConditions::resolve(cfg, c, error), "changed static load accepted");
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const auto kind = TestSequenceTaskKind::fromSettingsGroup(app.arguments().value(1));
        const auto mode = app.arguments().value(2, "success");
        require(TestSequenceTaskKind::isGroup(kind), "bad test kind");
        if (mode == "xml") { checkXml(kind); return 0; }
        QTemporaryDir directory;
        GroupState group;
        const auto results = executeGroup(kind, mode, false, group, directory.path());
        const auto &result = results.last();
        if (mode.startsWith("dual") && mode != "dual") {
            require(result.status != "Pass", "dual failure reported Pass");
            if (mode == "dual_missing" || mode == "dual_bad_index")
                require(group.trace.isEmpty() && result.attempt == 0, "invalid dual input touched hardware");
            else {
                require(group.trace.contains("input off") && group.trace.contains("dc off"), "dual failure skipped cleanup");
                if (mode == "dual_cancel") require(!group.trace.contains("dc on"), "cancelled AC startup enabled DC");
                // Exercise the same fault through the Single dispatcher as well.
                GroupState singles; auto failed = executeGroup(kind, mode, true, singles, directory.path());
                require(failed.first().status != "Pass" && singles.trace.contains("dc off"), "Single dual failure missed cleanup");
            }
            return 0;
        }
        if (mode == "preflight") {
            require(group.trace.isEmpty() && result.status == "Fail" && result.attempt == 0, "invalid later member touched hardware");
        } else if (mode == "failure" || mode == "cancel" || mode == "cleanup_contract") {
            require(results.size() == 1 && result.status != "Pass" && result.conditions.size() == 3 && result.conditions[1].status != "Pass"
                && result.conditions[2].status == "Skipped", "incomplete group passed, continued or retried");
            // Completed directions retain both event and independent reference results,
            // even when a later member fault changes the overall Group status.
            require(result.conditions[0].channels.size() == 2 && result.conditions[1].channels.size() == 2 &&
                    result.conditions[2].channels.isEmpty(), "Group partial/Skipped channel data incorrect");
            if (group.base == "Turn off" || group.base == "Turn on then short")
                for (int index : {0, 1}) {
                    const auto &condition = result.conditions[index];
                    for (const auto &ch : condition.channels) {
                        const bool event = ch.direction == (index == 0 ? MeasurementDirection::Fall : MeasurementDirection::Rise);
                        require(ch.source == (event ? MeasurementSource::Event : MeasurementSource::SteadyReference) &&
                                (event ? ch.sourceSequence > 0 : ch.sourceSequence == 0) &&
                                ch.rms == 1 + condition.condition.index && ch.mean == .5 + condition.condition.index,
                                "failed Group lost reference/event provenance or quartet");
                    }
                }
            require(result.conditions[0].channels[0].maximum == 4 && result.conditions[0].channels[0].minimum == -1 &&
                    result.conditions[1].channels[0].maximum == 3 && result.conditions[1].channels[0].minimum == -3,
                    "failed/cancelled Group mixed per-load extrema");
            if (TestSequenceTaskKind::isSteady(kind)) {
                require(result.channels.isEmpty() && result.conditions[0].channels[0].rms == 1 &&
                        result.conditions[0].channels[0].mean == .5 && result.conditions[1].channels[0].rms == 3 &&
                        result.conditions[1].channels[0].mean == 2.5 && result.conditions[2].channels.isEmpty(),
                        "failed/cancelled Group lost member data or filled Skipped values");
            }
        } else {
            require(result.status == "Pass" && result.conditions.size() == 2, qPrintable(result.error));
            const bool transient = TestSequenceTaskKind::isTransient(kind);
            require(result.conditions[0].channels[0].maximum == 4 && result.conditions[0].channels[0].minimum == -1 &&
                    result.conditions[1].channels[0].maximum == 3 && result.conditions[1].channels[0].minimum == -3,
                    "Group did not retain independent load extrema");
            if (transient) {
                for (const auto &condition : result.conditions)
                    for (const auto &ch : condition.channels)
                        require(ch.rms == 1 + condition.condition.index && ch.mean == .5 + condition.condition.index,
                                "transient quartet lost or mixed across loads");
                if (group.base == "Turn off" || group.base == "Turn on then short")
                    for (const auto &condition : result.conditions) {
                        require(condition.channels.size() == 2, "missing steady-reference direction");
                        const auto &reference = condition.channels[condition.condition.index == 0 ? 0 : 1];
                        const auto &event = condition.channels[condition.condition.index == 0 ? 1 : 0];
                        require(reference.source == MeasurementSource::SteadyReference && reference.sourceSequence == 0 &&
                                event.source == MeasurementSource::Event && event.sourceSequence > 0,
                                "steady reference promoted to an event or lost in service");
                    }
            } else {
                require(result.channels.isEmpty(), "steady Group contains mixed values");
                require(result.conditions[0].channels[0].maximum == 4 && result.conditions[0].channels[0].minimum == -1 &&
                        result.conditions[0].channels[0].rms == 1 && result.conditions[0].channels[0].mean == .5,
                        "first load values changed");
                require(result.conditions[1].channels[0].maximum == 3 && result.conditions[1].channels[0].minimum == -3 &&
                        result.conditions[1].channels[0].rms == 3 && result.conditions[1].channels[0].mean == 2.5,
                        "second load values changed");
            }
            if (mode == "success" || mode == "dual") {
                GroupState singles;
                auto singleResults = executeGroup(kind, mode, true, singles, directory.path());
                require(group.trace == singles.trace, "group changed instrument operations versus same singles");
                require(singleResults.size() == result.conditions.size(), "Group/Single member count differs");
                for (qsizetype i = 0; i < singleResults.size(); ++i) {
                    const auto &single = singleResults[i].channels;
                    const auto &member = result.conditions[i].channels;
                    require(singleResults[i].status == result.conditions[i].status && single.size() == member.size(),
                            "Group member result differs from Single");
                    for (qsizetype c = 0; c < single.size(); ++c) {
                        const auto same = [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); };
                        require(single[c].channel == member[c].channel && same(single[c].maximum, member[c].maximum) &&
                                same(single[c].minimum, member[c].minimum) && same(single[c].rms, member[c].rms) &&
                                same(single[c].mean, member[c].mean) && single[c].source == member[c].source &&
                                single[c].sourceSequence == member[c].sourceSequence,
                                "Group member metrics/source differ from Single");
                    }
                }
                require(group.periods == (TestSequenceTaskKind::isSteady(kind) ? 2 : 0), "Auto Period dispatch changed");
                if (mode == "dual") {
                    require(group.trace.count("input on") > 0 && group.trace.count("input on") == group.trace.count("dc on"), "not every power-on event enabled both sources");
                    require(group.trace.count("input off") == group.trace.count("dc off"), "not every power-off event disabled both sources");
                    for (int i = 0; i < group.trace.size(); ++i)
                        if (group.trace[i] == "input on" || group.trace[i] == "input off")
                            require(group.trace.value(i+1) == (group.trace[i] == "input on" ? "dc on" : "dc off"), "AC/DC order changed");
                }
                if ((group.base == "Turn off" || group.base == "Turn on then short") && mode == "success")
                    for (bool single : {false, true}) {
                        GroupState noEvent;
                        const auto fallbackResults = executeGroup(kind, "no_event", single, noEvent, directory.path());
                        for (const auto &fallback : fallbackResults) {
                            require(fallback.status == "Pass" && fallback.trials.isEmpty(), "no-event service result failed");
                            const auto verify = [](const QVector<TestSequenceChannelResult> &channels, int load) {
                                require(channels.size() == 2, "service lost a fallback direction");
                                for (const auto &ch : channels)
                                    require(ch.source == MeasurementSource::SteadyReference && ch.sourceSequence == 0 &&
                                            ch.maximum == (load == 0 ? 4 : 3) && ch.minimum == (load == 0 ? -1 : -3) &&
                                            ch.rms == 1 + load && ch.mean == .5 + load,
                                            "service lost/mixed reference quartet");
                            };
                            if (single) verify(fallback.channels, fallback.loadCondition.index);
                            else for (const auto &condition : fallback.conditions) {
                                require(condition.trials.isEmpty(), "reference became a Group Trial");
                                verify(condition.channels, condition.condition.index);
                            }
                        }
                    }
            }
            if (mode == "retry" || mode == "capture_retry") {
                require(results.size() == 2 && results[0].status == "Fail" && group.measures == 4, "retry did not restart whole group");
                require(results[0].conditions[0].status == "Pass" && results[0].conditions[1].status == "Fail" &&
                        result.conditions[0].status == "Pass" && result.conditions[1].status == "Pass" &&
                        results[0].conditions[0].channels[0].maximum == 4 && results[0].conditions[1].channels[0].minimum == -3,
                        "retry overwrote previous member results/status");
            }
            if (mode == "capture" || mode == "capture_retry") {
                require(!result.files.isEmpty(), "capture missing");
                bool first = false, second = false;
                for (const auto &file : result.files) {
                    first |= file.contains("Load1"); second |= file.contains("Load3");
                    require(file.endsWith(".png") || file.endsWith(".wfm"),"unselected format saved");
                }
                require(first && second, "capture records lost condition identity");
                checkCaptureFolders(results, directory.path(), TestSequenceTaskKind::name(kind));
                // Two Single tasks must use their own row numbers and retain both rows' files.
                QTemporaryDir singleDirectory;
                GroupState singles;
                const auto singleResults = executeGroup(kind, mode, true, singles, singleDirectory.path());
                require(singleResults.size() == (mode == "capture_retry" ? 3 : 2), "Single capture attempts missing");
                checkCaptureFolders(singleResults, singleDirectory.path(), singles.base);
            }
        }
        std::cout << "PASS " << app.arguments().join(' ').toStdString();
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
