#include "instrumentwithcommbase.h"
#include "transfercancellation.h"
#include <QThread>
#include <stdexcept>

struct Transport : ICommunication {
    QByteArray response = "#16abcdef\n";
    int limit = 100;
    int reads = 0;
    int failAt = -1;
    int delay = 0;
    QAtomicInt *cancel = nullptr;
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return {}; }
    void setTimeoutMs(int ms) override { limit = ms; }
    int timeoutMs() const override { return limit; }
    int write(const QByteArray &bytes) override { return int(bytes.size()); }
    int read(QByteArray &bytes, int maximum) override {
        if (++reads == failAt) return -1;
        if (delay) QThread::msleep(delay);
        bytes = response.left(qMin(maximum, 3));
        response.remove(0, bytes.size());
        if (cancel) cancel->storeRelease(1);
        return int(bytes.size());
    }
};
struct Instrument : InstrumentWithCommBase {
    using InstrumentWithCommBase::InstrumentWithCommBase;
    using InstrumentWithCommBase::queryBinary;
    QString model() const override { return "fake"; }
    QString vendor() const override { return "fake"; }
};
void require(bool value) { if (!value) throw std::runtime_error("binary transfer contract failed"); }
int main() {
    for (int mode = 0; mode < 4; ++mode) {
        Transport comm;
        Instrument instrument(&comm);
        QAtomicInt stop = 0;
        if (mode == 1) comm.failAt = 3;
        if (mode == 2) { comm.limit = 10; comm.delay = 6; }
        if (mode == 3) comm.cancel = &stop;
        const int original = comm.limit;
        QByteArray bytes = "old";
        {
            TransferCancellation guard(stop);
            const bool ok = instrument.queryBinary("CURVE?", bytes);
            require(ok == (mode == 0));
            require(mode == 0 ? bytes == "abcdef" : bytes.isEmpty());
            require(comm.limit == original);
        }
        require(!TransferCancellation::requested());
    }
}
