#define main original_followup_main
#include "../../integration/review_followup_probe.cpp"
#undef main
#include "testrunservice.h"
#include "testsequenceconditionmapping.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        for (const QString fault : {"unmapped", "disabled", "address", "model", "index", "value"}) {
            TestSequenceExecutionContext ctx;
            ctx.instrumentSetup = config("Relay"); ctx.instrumentSetup.instruments.resize(1); ctx.instrumentSetup.relayOutputs = 2;
            ctx.relays = {{"off", {"OFF", "OFF"}}, {"on", {"OFF", "ON"}}};
            auto &inst = ctx.instrumentSetup.instruments[0];
            if (fault != "unmapped") { inst.channels.append({"fake", 2, 0}); inst.channelNumbers.append(2); }
            if (fault == "disabled") inst.enabled = false;
            if (fault == "address") inst.commConfig = {};
            if (fault == "model") inst.modelName.clear();
            if (fault == "value") ctx.relays[1].values[1] = "INVALID";
            const int badIndex = fault == "index" ? 3 : 1;
            TaskPayload first; first.task.name = "Relay"; first.settings = {{"relay_index", 0}};
            TaskPayload bad = first; bad.settings["relay_index"] = badIndex;
            TestRunDependencies dependencies; dependencies.instruments.relay = InstrumentExecutor::runRelay;
            TestRunService service(dependencies); QVector<TestSequenceResultRecord> records;
            QObject::connect(&service, &TestRunService::resultRecordsReady, [&](const auto &r) { records = r; });
            trace.clear(); service.startTasks({first, bad}, ctx);
            require(records.size() == 2 && records[0].status == "Skipped" && records[1].status == "Fail"
                        && records[1].attempt == 0 && trace.isEmpty(), "bad later Relay began batch IO");
            const auto direct = InstrumentExecutor::runRelay(ctx.instrumentSetup, ctx.relays, badIndex, RelayAction::RelayOn);
            require(!direct.success && direct.outputUnchanged && trace.isEmpty(), "Executor mapping bypass/partial IO");
            if (fault == "value" || fault == "index") {
                const auto off = InstrumentExecutor::runRelay(ctx.instrumentSetup, ctx.relays, badIndex, RelayAction::RelayOff);
                require(off.success && trace.join('|').contains(":OFF"), "invalid condition blocked emergency OFF");
            }
        }
        auto cfg = config("Relay"); cfg.instruments.resize(1); cfg.relayOutputs = 2;
        cfg.instruments[0].channels.append({"fake", 2, 0}); cfg.instruments[0].channelNumbers.append(2);
        trace.clear();
        const auto on = InstrumentExecutor::runRelay(cfg, {{"valid", {"OFF", "ON"}}}, 0, RelayAction::RelayOn);
        require(on.success && trace.join('|').contains(":ON"), "valid mapped Relay rejected");
        std::cout << "PASS Relay preflight/Executor/OFF scenarios\n";
    } catch (const std::exception &e) { std::cerr << e.what() << "\n"; return 1; }
}
