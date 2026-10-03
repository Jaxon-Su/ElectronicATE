#include "chroma62000.h"
#include "dcsourcefactory.h"
#include <QCoreApplication>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

class Transport : public ICommunication {
public:
    QList<QByteArray> writes, replies;
    int writeResult = -2;
    int reads = 0;
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return {}; }
    int write(const QByteArray& bytes) override {
        writes.append(bytes);
        return writeResult == -2 ? bytes.size() : writeResult;
    }
    int read(QByteArray& bytes, int) override {
        ++reads;
        if (replies.isEmpty()) return -1;
        bytes = replies.takeFirst();
        return bytes.size();
    }
};
void check(bool result) { if (!result) throw std::runtime_error("DC Source assertion failed"); }
template<class F> void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        Transport comm;
        check(Chroma62000Spec::supportedModels().size() == 13);
        for (const auto& name : Chroma62000Spec::supportedModels()) {
            const auto spec = Chroma62000Spec::forModel(name);
            std::unique_ptr<DCSource> source(DCSourceFactory::createDCSource(name, &comm));
            check(source && source->model() == name && source->vendor() == "Chroma");
            source->setVoltage(0);
            source->setVoltage(spec->maxVoltage);
            source->setCurrent(0);
            source->setCurrent(spec->maxCurrent);
            const auto count = comm.writes.size();
            rejects([&] { source->setVoltage(spec->maxVoltage + 1); });
            rejects([&] { source->setCurrent(spec->maxCurrent + 1); });
            rejects([&] { source->setVoltage(-1); });
            rejects([&] { source->setCurrent(-1); });
            rejects([&] { source->setVoltage(std::numeric_limits<double>::quiet_NaN()); });
            rejects([&] { source->setCurrent(std::numeric_limits<double>::infinity()); });
            check(comm.writes.size() == count);
        }
        check(Chroma62000Spec::forModel("62100H-30")->maxPower == 11250);
        check(!DCSourceFactory::createDCSource("62000H", &comm));
        check(!DCSourceFactory::createDCSource("62050P-100", &comm));
        check(!DCSourceFactory::createDCSource("62050H-40", nullptr));
        rejects([&] { Chroma62000 invalid("unknown", &comm); });
        Chroma62000 source(" 62050h-40 ", &comm);
        comm.writes.clear();
        source.setVoltage(12.5);
        source.setCurrent(2.5);
        source.setPowerOn();
        comm.replies = {"ON\n", "OFF\n"};
        source.setPowerOff();
        comm.replies = {"1.25e+01\n", "2.5\r\n", "31.25\n"};
        check(source.measureVoltage() == 12.5);
        check(source.measureCurrent() == 2.5);
        check(source.measurePower() == 31.25);
        check(comm.writes == QList<QByteArray>{"SOUR:VOLT 12.5\n", "SOUR:CURR 2.5\n",
            "CONF:OUTP ON\n", "CONF:OUTP OFF\n", "CONF:OUTP?\n", "CONF:OUTP?\n",
            "MEAS:VOLT?\n", "MEAS:CURR?\n", "MEAS:POW?\n"});
        for (const auto& reply : QList<QByteArray>{"", "nan", "inf", "error 0", "0 extra"}) {
            comm.replies = {reply};
            rejects([&] { source.measureVoltage(); });
            check(!source.lastError().isEmpty());
        }
        comm.replies = {"ON", "ON", "ON"};
        rejects([&] { source.setPowerOff(); });
        for (int result : {-1, 0, 2}) {
            comm.writeResult = result;
            rejects([&] { source.setVoltage(1); });
            const int before = comm.reads;
            rejects([&] { source.measureCurrent(); });
            rejects([&] { source.setPowerOff(); });
            check(comm.reads == before);
        }
        Chroma62000 noComm("62050H-40");
        rejects([&] { noComm.setPowerOn(); });
        rejects([&] { noComm.measurePower(); });
        std::cout << "PASS: 62000H models, SCPI, limits and communication failures\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
