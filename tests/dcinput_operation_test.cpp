#include "dcinputoperation.h"
#include "icommunication.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

struct State { QStringList resources; QList<QByteArray> writes; int closes = 0; bool fail = false; QByteArray failCommand; };
class Comm : public ICommunication {
public:
    explicit Comm(State& state) : s(state) {}
    State& s;
    bool opened = false;
    bool open() override { opened = true; return true; }
    void close() override { opened = false; ++s.closes; }
    bool isOpen() const override { return opened; }
    QString lastError() const override { return {}; }
    int write(const QByteArray& data) override { s.writes.append(data); return s.fail || data == s.failCommand ? -1 : data.size(); }
    int read(QByteArray& data, int) override { data = "OFF\n"; return data.size(); }
};
void check(bool value) { if (!value) throw std::runtime_error("DC input operation assertion failed"); }
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        State s;
        auto create = [&](const QString& resource) -> ICommunication* {
            s.resources.append(resource); return new Comm(s);
        };
        Page1Config config;
        config.dcInputs = 3;
        for (int i = 0; i < 3; ++i) {
            InstrumentConfig instrument;
            instrument.name = QString("DC Source%1").arg(i + 1);
            instrument.type = "InputDCSource";
            instrument.modelName = "62050H-40";
            instrument.address = QString("GPIB0::%1::INSTR").arg(i + 1);
            config.instruments.append(instrument);
        }
        for (int i = 0; i < 3; ++i) {
            s.writes.clear();
            check(runDcInput(config, i, {"12", "2", ""}, InputAction::PowerOn, create).success);
            check(s.resources.last() == config.instruments[i].address);
            check(s.writes == QList<QByteArray>{"SOUR:CURR 2\n", "SOUR:VOLT 12\n", "CONF:OUTP ON\n"});
            s.writes.clear();
            check(runDcInput(config, i, {"5", "1", ""}, InputAction::Change, create).success);
            check(s.writes == QList<QByteArray>{"SOUR:CURR 1\n", "SOUR:VOLT 5\n"});
            s.writes.clear();
            check(runDcInput(config, i, {}, InputAction::PowerOff, create).success);
            check(s.writes == QList<QByteArray>{"CONF:OUTP OFF\n", "CONF:OUTP?\n"});
        }
        check(s.closes == 9);
        const int opened = s.resources.size();
        for (const auto& row : QVector<DcRow>{{"", "2", ""}, {"12", "", ""}, {"41", "2", ""}, {"12", "126", ""}, {"nan", "2", ""}})
        {
            const auto result = runDcInput(config, 0, row, InputAction::PowerOn, create);
            check(!result.success && result.outputUnchanged);
        }
        config.instruments[0].enabled = false;
        check(!runDcInput(config, 0, {}, InputAction::PowerOff, create).success);
        config.instruments[0].enabled = true;
        config.instruments[0].modelName = "TBD";
        check(!runDcInput(config, 0, {}, InputAction::PowerOff, create).success);
        check(!runDcInput(config, 3, {}, InputAction::PowerOff, create).success);
        check(s.resources.size() == opened);
        s.fail = true;
        check(!runDcInput(config, 1, {"12", "2", ""}, InputAction::PowerOn, create).success);
        check(s.closes == 10);
        s.fail = false;
        config.instruments[0].modelName = "62050H-40";
        DcGroup group = {{{"24", "5", ""}, {"12", "3", ""}, {"5", "1", ""}}};
        s.writes.clear();
        auto grouped = runDcGroup(config, group, InputAction::PowerOn, create);
        check(grouped.success && grouped.confirmedOutput == true);
        check(s.writes == QList<QByteArray>{"SOUR:CURR 5\n", "SOUR:VOLT 24\n",
            "SOUR:CURR 3\n", "SOUR:VOLT 12\n", "SOUR:CURR 1\n", "SOUR:VOLT 5\n",
            "CONF:OUTP ON\n", "CONF:OUTP ON\n", "CONF:OUTP ON\n"});
        s.writes.clear();
        group[2].vin = "99";
        grouped = runDcGroup(config, group, InputAction::PowerOn, create);
        check(!grouped.success && grouped.outputUnchanged && s.writes.isEmpty());
        group[2].vin = "5";
        grouped = runDcGroup(config, group, InputAction::Change, create);
        check(grouped.success && s.writes.size() == 6 && !s.writes.contains("CONF:OUTP ON\n"));
        s.writes.clear();
        s.failCommand = "CONF:OUTP ON\n";
        grouped = runDcGroup(config, group, InputAction::PowerOn, create);
        check(!grouped.success && grouped.confirmedOutput == false && s.writes.count("CONF:OUTP OFF\n") == 3);
        s.writes.clear();
        s.failCommand = "CONF:OUTP OFF\n";
        grouped = runDcGroup(config, {}, InputAction::PowerOff, create);
        check(!grouped.success && !grouped.confirmedOutput && s.writes.count("CONF:OUTP OFF\n") == 3);
        s.failCommand.clear();
        config.instruments[1].enabled = false;
        s.writes.clear();
        grouped = runDcGroup(config, group, InputAction::PowerOn, create);
        check(grouped.success && s.writes.count("CONF:OUTP ON\n") == 2);
        config.dcInputs = 1;
        config.instruments[1].enabled = true;
        group[1] = {}; group[2] = {};
        s.writes.clear();
        grouped = runDcGroup(config, group, InputAction::PowerOn, create);
        check(grouped.success && s.writes.count("CONF:OUTP ON\n") == 1);
        s.writes.clear();
        grouped = runDcGroup(config, {}, InputAction::PowerOff, create);
        check(grouped.success && s.writes.count("CONF:OUTP OFF\n") == 1);
        std::cout << "PASS: DC routing, ON/Change/OFF, validation and cleanup\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
