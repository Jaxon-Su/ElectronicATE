#include "instrumentwithcommbase.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
class Transport : public ICommunication {
public:
    QList<QByteArray> replies;
    bool failWrite = false;
    int reads = 0;
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return {}; }
    int write(const QByteArray& data) override { return failWrite ? -1 : data.size(); }
    int read(QByteArray& data, int) override {
        ++reads;
        if (replies.isEmpty()) return -1;
        data = replies.takeFirst();
        return data.size();
    }
};
class Instrument : public InstrumentWithCommBase {
public:
    using InstrumentWithCommBase::InstrumentWithCommBase;
    using InstrumentWithCommBase::requireOutputOff;
    QString model() const override { return "offline"; }
    QString vendor() const override { return "offline"; }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        Transport transport;
        Instrument instrument(&transport);
        for (const auto& valid : QList<QByteArray>{"OFF\n", "0\n", "+0.0\n"}) {
            transport.replies = {valid};
            instrument.requireOutputOff("OUTPut?");
        }
        transport.replies = {"1", "ON", "0"};
        instrument.requireOutputOff("LOAD?");
        for (const auto& invalid : QList<QByteArray>{"", "garbage", "0.5", "nan", "0 extra", "1"}) {
            transport.replies = {invalid, invalid, invalid};
            bool rejected = false;
            try { instrument.requireOutputOff("OUTPut?"); } catch (const std::exception&) { rejected = true; }
            if (!rejected) throw std::runtime_error("unconfirmed OFF accepted");
        }
        transport.failWrite = true;
        const int before = transport.reads;
        bool rejected = false;
        try { instrument.requireOutputOff("LOAD?"); } catch (const std::exception&) { rejected = true; }
        if (!rejected || before != transport.reads) throw std::runtime_error("failed query write accepted");
        std::cout << "PASS: OFF readback, delayed state and invalid responses\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
