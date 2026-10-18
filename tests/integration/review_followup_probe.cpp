#include "instrumentexecutor.h"
#include "communicationfactory.h"
#include "relayfactory.h"
#include "relay.h"
#include "dcloadfactory.h"
#include "acsourcefactory.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

static QStringList trace;
static QString fault, faultMode;
class ProbeComm : public ICommunication {
public:
    QString id; bool opened = false;
    explicit ProbeComm(QString id) : id(std::move(id)) {}
    bool open() override { trace << id + ":OPEN"; if (id == fault) { if (faultMode == "open_unknown") throw 42; if (faultMode == "open_false") return false; if (faultMode == "open") throw std::runtime_error("injected open exception"); } return opened = true; }
    bool isOpen() const override { return opened; }
    void close() override { opened = false; }
    int write(const QByteArray &b) override { return b.size(); }
    int read(QByteArray &data, int) override { data = "OFF"; return data.size(); }
    QString lastError() const override { return {}; }
};
class ProbeRelay : public RelayBase {
public:
    QString id;
    explicit ProbeRelay(ProbeComm *c) : RelayBase(c), id(c->id) { setMaxChannels(2); }
    QString model() const override { return "fake"; }
    QString vendor() const override { return "fake"; }
    bool turnOn(int) override { trace << id + ":ON"; return true; }
    bool turnOff(int) override { trace << id + ":OFF"; if (id == fault && faultMode == "off_unknown") throw 42; return true; }
    bool setMultiChannels(const QVector<int> &, bool) override { return true; }
    RelayStatus getStatus(int) override { return trace.contains(id + ":ON") ? RelayStatus::Closed : RelayStatus::Open; }
};
class ProbeLoad : public DCLoad {
public:
    QString id;
    explicit ProbeLoad(ProbeComm *c) : DCLoad(c), id(c->id) {}
    QString model() const override { return "fake"; }
    QString vendor() const override { return "fake"; }
    void setLoadOn() override { trace << id + ":ON"; }
    void setLoadOff() override { trace << id + ":OFF"; }
    void setStaticCurrent(const StaticCurrentParam &) override {}
    void setDynamicCurrent(const DynamicCurrentParam &) override {}
};
ICommunication *CommunicationFactory::create(const QString &id) { if (id == fault && faultMode == "create") throw std::runtime_error("create failed"); if (id == fault && faultMode == "create_unknown") throw 42; return new ProbeComm(id); }
RelayBase *RelayFactory::createRelay(const QString &, ICommunication *c, quint8) { return new ProbeRelay(static_cast<ProbeComm *>(c)); }
DCLoad *DCLoadFactory::createDCLoad(const QString &model, ICommunication *c) { return model == "unsupported" ? nullptr : new ProbeLoad(static_cast<ProbeComm *>(c)); }
ACSource *ACSourceFactory::createACSource(const QString &, ICommunication *) { return nullptr; }

static InstrumentSetupConfig config(QString type) {
    InstrumentSetupConfig c;
    for (int n = 1; n <= 3; ++n) {
        InstrumentConfig i; i.enabled = true; i.name = type + QString::number(n); i.type = type; i.modelName = "fake";
        i.commConfig.protocol = ProtocolType::GPIB; i.commConfig.gpibAddress = n;
        i.channels = {{"fake", n, 0}}; i.channelNumbers = {1}; c.instruments << i;
    }
    return c;
}
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        auto relays = config("Relay"); for (auto &i : relays.instruments) { i.channels.append(i.channels[0]); i.channelNumbers.append(2); }
        for (const QString mode : {"open", "open_unknown", "open_false", "create", "create_unknown", "off_unknown", "valid"}) {
            faultMode = mode; fault = mode == "valid" ? QString{} : relays.instruments[1].getResourceString(); trace.clear();
            const auto r = InstrumentExecutor::runRelay(relays, {}, -1, RelayAction::RelayOff);
            require(r.success == (mode == "valid"), "Relay OFF failure classification");
            require(trace.contains(relays.instruments[0].getResourceString() + ":OFF") &&
                    trace.contains(relays.instruments[2].getResourceString() + ":OFF"), "Healthy relay OFF skipped");
            if (mode != "valid") require(!r.confirmedOutput.has_value(), "Partial OFF falsely confirmed"); if (mode == "off_unknown") require(trace.count(fault + ":OFF") == 2, "Nonstandard exception skipped later channel");
            if (mode != "off_unknown") {
                trace.clear();
                const auto on = InstrumentExecutor::runRelay(relays, {{"on", {"ON", "ON", "ON"}}}, 0, RelayAction::RelayOn);
                require(on.success == (mode == "valid"), "Relay ON accepted partial initialization");
                if (mode != "valid") require(!trace.join('|').contains(":ON"), "Partial relay ON sent");
            }
            std::cout << "Relay " << mode.toStdString() << " passed\n";
        }
        fault.clear(); faultMode.clear();
        for (bool dynamic : {false, true}) for (const QString mode : {"missing", "unsupported", "unmapped", "valid"}) {
            auto loads = config("Load");
            if (mode == "missing" || mode == "unmapped") loads.instruments[1].channels[0].subModel.clear();
            if (mode == "unsupported") loads.instruments[1].channels[0].subModel = "unsupported";
            if (mode == "unmapped") loads.instruments[1].channels[0].index = 0;
            trace.clear(); DynamicMetaRow meta; meta.t1t2 = {"100~100"};
            const auto result = dynamic
                ? InstrumentExecutor::runDynamicLoad(loads, {{"row", {"1", "1", "1"}}}, 0, meta, DynamicLoadAction::LoadOn)
                : InstrumentExecutor::runLoad(loads, {{"row", {"1", "1", "1"}}}, 0, {}, LoadAction::LoadOn);
            const bool valid = mode == "valid" || mode == "unmapped";
            require(result.success == valid, "Load incomplete configuration classification");
            if (!valid) require(result.outputUnchanged && !trace.join('|').contains(":ON"), "Invalid load configuration sent ON");
            else require(trace.contains(loads.instruments[0].getResourceString() + ":ON") &&
                         trace.contains(loads.instruments[2].getResourceString() + ":ON"), "Valid load channels not enabled");
            trace.clear();
            const auto off = dynamic ? InstrumentExecutor::runDynamicLoad(loads, {}, -1, {}, DynamicLoadAction::LoadOff)
                                     : InstrumentExecutor::runLoad(loads, {}, -1, {}, LoadAction::LoadOff);
            require(trace.contains(loads.instruments[0].getResourceString() + ":OFF") &&
                    trace.contains(loads.instruments[2].getResourceString() + ":OFF"), "Load cleanup skipped healthy devices");
            require(off.success == (mode == "valid"), "Load cleanup incomplete configuration classification");
            std::cout << "Load dynamic=" << dynamic << ' ' << mode.toStdString() << " passed\n";
        }
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
