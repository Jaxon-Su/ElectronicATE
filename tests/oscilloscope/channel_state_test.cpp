#include "msoseries456.h"
#include "descentstrategy.h"
#include "transientstrategy.h"
#include "steadystatesearchstrategy.h"
#include <QCoreApplication>
#include <QSet>
#include <iostream>
#include <stdexcept>

// Real MSO channel-state parser and real algorithms; acquisition and outputs are fake.
class Transport final : public ICommunication {
public:
    QString mode; int target = 2, failAt = 1, reads = 0, failedReads = 0; QAtomicInt *cancel = nullptr;
    QString last;
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return "injected CH2 display-state read failure"; }
    int write(const QByteArray &bytes) override { last = QString::fromUtf8(bytes).trimmed(); return bytes.size(); }
    int read(QByteArray &bytes, int) override {
        if (!last.startsWith("DISplay:GLObal:CH") || !last.endsWith(":STATE?"))
            throw std::runtime_error("Unexpected transport command: " + last.toStdString());
        if (last == QString("DISplay:GLObal:CH%1:STATE?").arg(target)) {
            ++reads;
            if (cancel) cancel->storeRelease(1);
            if (mode == "off") { bytes = "OFF"; return bytes.size(); }
            if (reads >= failAt && (mode == "repeat" || reads == failAt)) {
                if (mode == "timeout" || mode == "repeat") { ++failedReads; bytes.clear(); return -1; }
                if (mode == "invalid") { ++failedReads; bytes = "INVALID"; return bytes.size(); }
            }
        }
        bytes = "1"; return 1;

    }
};

class Scope final : public MSOSeries456 {
public:
    explicit Scope(Transport &transport) : MSOSeries456(&transport) {}
    bool steady = false, automatic = false, armed = false, completed = false;
    double level = 0;
    ScopeTriggerSlope slope = ScopeTriggerSlope::Rise;
    QSet<int> measured; int stops = 0, outputOn = 0;
    QString getTriggerSource() override { return "CH4"; }
    bool writeConfigurationCommand(const QString &, QString &error) override { error.clear(); return true; }
    int triggerChannel() override { return 4; }
    double getChannelScale(int) override { return 10; }
    bool isClipping(int) override { return false; }
    bool stopAcquisition(QString &error) override { ++stops; armed = false; error.clear(); return true; }
    void stop() override { ++stops; armed = false; }
    bool selectEdgeTrigger(QString &error) override { error.clear(); return true; }
    bool setEdgeTriggerChannel(int channel, QString &error) override { error.clear(); return channel == 4; }
    bool setEdgeSlope(ScopeTriggerSlope value, QString &error) override { slope = value; error.clear(); return true; }
    bool setTriggerMode(ScopeTriggerMode value, QString &error) override { automatic = value == ScopeTriggerMode::Auto; error.clear(); return true; }
    bool setChannelTriggerLevel(int, double value, QString &error) override { level = value; error.clear(); return true; }
    bool beginSingleAcquisition() override { armed = true; completed = automatic || (steady && hits()); return true; }
    ScopeTriggerState triggerState() override { return ScopeTriggerState::Ready; }
    ScopeAcquisitionState acquisitionState() override { return completed ? ScopeAcquisitionState::Completed : ScopeAcquisitionState::Running; }
    double readMeasurement(int channel, ScopeMeasurement metric) override {
        measured.insert(channel); m_lastError.clear();
        switch(metric) {
        case ScopeMeasurement::Maximum:return automatic ? 10 : 20;
        case ScopeMeasurement::Minimum:return 0;
        case ScopeMeasurement::Rms:return 5;
        case ScopeMeasurement::Mean:return 4;
        }
        return 0;
    }
    bool hits() const { return slope == ScopeTriggerSlope::Rise ? level <= 20 : level >= 0; }
    void fire() { if (armed && hits()) completed = true; }
};


void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void check(int kind, const QString &mode, int channel = 2, int failAt = 1) {
    Transport transport; transport.mode = mode; transport.target = channel; transport.failAt = failAt;
    Scope scope(transport); QAtomicInt stop{0}; if (mode == "stop") transport.cancel = &stop;
    TransientContext cfg; cfg.target = "MAX"; cfg.trialsPerLevel = 1;
    cfg.settleMs = cfg.dischargeMs = cfg.powerOffMs = 0; cfg.timeoutMs = 5; cfg.phaseTimeoutMs = 10000;
    cfg.descentChannel = 4; cfg.descentLevels = {5};
    cfg.power = [&](bool on, QString &) { if (on) ++scope.outputOn; scope.fire(); return true; };
    cfg.load = [&](bool on, QString &) { if (on) ++scope.outputOn; return true; };
    cfg.relays = [&](bool sh, bool, QString &) { if (sh) scope.fire(); return true; };
    OscMeasureResult result;
    if (kind == 4 || kind == 5) { DescentStrategy strategy(kind == 5, cfg); result = strategy.execute(&scope, stop); }
    else if (kind == 6) { scope.steady = true; SteadyStateSearchStrategy strategy(5, 10000, 100, "MAX"); result = strategy.execute(&scope, stop); }
    else result = runTransientStrategy(static_cast<TransientKind>(kind), cfg, &scope, stop);
    std::cout << kind << " " << mode.toStdString() << " CH" << channel << " at=" << failAt
              << " success=" << result.success << " error=" << result.errorMessage.toStdString() << '\n';
    const bool fails = mode != "normal" && mode != "off";
    if (fails) {
        require(!result.success, "channel-state failure or Stop reported success");
        if (mode == "stop") require(result.errorMessage.contains("Stopped"), "Stop not propagated");
        else require(transport.failedReads > 0 && result.errorMessage.contains(QString("CH%1").arg(channel)), "channel failure not identified");
        require(result.trials.isEmpty(), "failed first event produced a valid Trial");
        if (kind == 6 || failAt > 1) require(scope.stops > 0, "scope cleanup omitted");
        if (failAt == 1) require(result.trials.isEmpty() && scope.outputOn == 0, "initial fault started output or valid Trial");
    } else {
        QSet<int> channels;
        for (const auto &trial : result.trials) for (const auto &value : trial.channels) channels.insert(value.channel);
        require(result.success && !result.trials.isEmpty(), "normal strategy failed");
        require(channels.size() == (mode == "off" ? 3 : 4), "wrong enabled channel set");
        require(channels.contains(2) == (mode != "off"), "legal OFF was not preserved");
    }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc > 1 && QString::fromUtf8(argv[1]) == "startup") {
            for (const QString mode : {"normal", "off", "timeout", "invalid"}) {
                Transport transport; transport.mode = mode; transport.target = 4; Scope scope(transport);
                QAtomicInt stop{0}; QString error;
                const bool ok = scope.prepareSteadyAcquisition(stop, error, false);
                require(ok == (mode == "normal"), "startup state handling");
                if (mode == "timeout" || mode == "invalid")
                    require(error.contains("CH4") && error.contains("STATE", Qt::CaseInsensitive), "startup lost channel/query error");
            }
            return 0;
        }
        const int kind = argc > 1 ? QString::fromUtf8(argv[1]).toInt() : 4;
        check(kind, "normal"); check(kind, "off"); check(kind, "stop");
        for (int channel : {2, 4}) for (const QString mode : {"timeout", "repeat", "invalid"}) check(kind, mode, channel);
        // Later enumeration in reference / completed-record clipping must also fail.
        if (kind != 4 && kind != 5) for (const QString mode : {"timeout", "invalid"}) check(kind, mode, 2, 2);
        if (kind == 1 || kind == 6) {
            check(kind, "timeout", 2, 3); // AUTO reference values
            check(kind, "timeout", 2, 4); // First valid event
        }
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
