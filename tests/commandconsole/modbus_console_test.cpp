#include "../support/test_evidence.h"
#include "modbusrtucodec.h"
#include "modbusrtuprotocol.h"
#include "modbusrtuexchange.h"
#include "commandconsoleviewmodel.h"
#include "commandconsolemodel.h"
#include "../../src/views/commandconsole/commandconsolepage.h"
#include <QApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDir>
#include <QFontDatabase>
#include <QPointer>
#include <QShortcut>
#include <atomic>
#include <iostream>

void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
QByteArray framed(QByteArray body) {
    const auto crc = Modbus_RTU_Protocol::calcCRC(body);
    body.append(char(crc)); body.append(char(crc >> 8)); return body;
}
QByteArray reply(int function, int slave = 1) {
    QByteArray body; body.append(char(slave)); body.append(char(function));
    if (function <= 2) body.append(QByteArray::fromHex("0101"));
    else if (function <= 4) body.append(QByteArray::fromHex("04000aff00"));
    else if (function == 5) body.append(QByteArray::fromHex("0000ff00"));
    else if (function == 6) body.append(QByteArray::fromHex("00001234"));
    else body.append(QByteArray::fromHex("00000002"));
    return framed(body);
}
ModbusRequest request(int function) {
    ModbusRequest r{1, function, 0, function == 5 || function == 6 ? 1 : 2, {}};
    if (function == 5) r.values = {1};
    if (function == 6) r.values = {0x1234};
    if (function == 15) r.values = {1, 0};
    if (function == 16) r.values = {0x1234, 0xABCD};
    return r;
}
struct State {
    QByteArray response = reply(3), pending, sent, afterResponse;
    bool opened = true, clearOk = true, timedOut = false, readError = false, throwRead = false, throwWrite = false;
    bool wrongCount = false, wrongThread = false, gapInResponse = false;
    int chunkSize = 256, writes = 0, readsAfterWrite = 0, timeout = 777, writeCount = -2;
    int replyDelayMs = 0;
    QThread *owner = nullptr;
    std::atomic<int> destroyed{0};
};
class Transport : public ICommunication {
  public:
    explicit Transport(std::shared_ptr<State> state) : s(std::move(state)) { s->owner = QThread::currentThread(); }
    ~Transport() override { thread(); ++s->destroyed; }
    void thread() const { if (s->owner != QThread::currentThread()) s->wrongThread = true; }
    bool open() override { thread(); return s->opened = true; }
    void close() override { thread(); s->opened = false; }
    bool isOpen() const override { thread(); return s->opened; }
    QString lastError() const override { return "injected transport error"; }
    bool discardInput() override { thread(); s->pending.clear(); s->readsAfterWrite = 0; return s->clearOk; }
    int timeoutMs() const override { return s->timeout; }
    void setTimeoutMs(int ms) override { s->timeout = ms; }
    bool readTimedOut() const override { return s->timedOut; }
    int write(const QByteArray &bytes) override {
        thread(); if (s->throwWrite) throw std::runtime_error("write exception");
        s->sent = bytes; ++s->writes; s->pending = s->response;
        return s->writeCount == -2 ? bytes.size() : s->writeCount;
    }
    int read(QByteArray &bytes, int limit) override {
        thread(); s->timedOut = false;
        if (s->writes) ++s->readsAfterWrite;
        if (s->throwRead && s->writes) throw std::runtime_error("read exception");
        if (s->readError && s->writes) return -1;
        if (s->writes && s->replyDelayMs) QThread::msleep(s->replyDelayMs);
        if (s->gapInResponse && s->readsAfterWrite == 2) {
            QThread::msleep(s->timeout + 2); s->timedOut = true; return -1;
        }
        if (s->writes && s->pending.isEmpty() && !s->afterResponse.isEmpty()) {
            s->pending = s->afterResponse; s->afterResponse.clear();
        }
        if (!s->pending.isEmpty()) {
            bytes = s->pending.left(qMin(limit, s->chunkSize));
            s->pending.remove(0, bytes.size());
            return bytes.size() + (s->wrongCount ? 1 : 0);
        }
        QThread::msleep(s->timeout); s->timedOut = true; return -1;
    }
    std::shared_ptr<State> s;
};
void settle(CommandConsoleViewModel &vm) {
    QElapsedTimer timer; timer.start();
    while (vm.isOperationPending() && timer.elapsed() < 2500) { QApplication::processEvents(); QThread::msleep(1); }
    check(!vm.isOperationPending(), "background operation stranded");
}

void codecTests() {
    QString error, summary;
    check(ModbusRtuCodec::encode(request(5), error).toHex() == "01050000ff008c3a", "known FC05/CRC vector changed");
    check(ModbusRtuCodec::encode(request(3), error).toHex() == "010300000002c40b", "known FC03/CRC vector changed");
    for (int function : {1, 2, 3, 4, 5, 6, 15, 16}) {
        const auto r = request(function);
        const auto tx = ModbusRtuCodec::encode(r, error);
        check(!tx.isEmpty() && error.isEmpty(), "valid function rejected");
        check(ModbusRtuCodec::decode(r, reply(function), summary, error), qPrintable(error));
        check(!summary.isEmpty(), "decoded summary missing");
        if (function <= 2) check(summary.contains("0x0000 = 1") && summary.contains("0x0001 = 0"), "coil bit order changed");
        if (function == 3 || function == 4) check(summary.contains("10 (0x000a)") && summary.contains("65280 (0xff00)"), "register endianness or binary newline corrupted");
        if (function == 15) check(tx.first(8).toHex() == "010f000000020101", "FC0F packing wrong");
        if (function == 16) check(tx.first(11).toHex() == "011000000002041234abcd", "FC10 packing wrong");
        for (int mutation = 0; mutation < 5; ++mutation) {
            auto bad = reply(function);
            if (mutation == 0) bad[bad.size()-1] ^= 1;
            if (mutation == 1) bad = reply(function, 2);
            if (mutation == 2) { bad[1] = char(7); bad = framed(bad.first(bad.size()-2)); }
            if (mutation == 3) bad.chop(1);
            if (mutation == 4) bad.append('\n');
            check(!ModbusRtuCodec::decode(r, bad, summary, error), "invalid response accepted");
        }
        QByteArray exceptionBody;
        exceptionBody.append(char(1)); exceptionBody.append(char(function | 0x80)); exceptionBody.append(char(2));
        auto exception = framed(exceptionBody);
        check(!ModbusRtuCodec::decode(r, exception, summary, error) && error.contains("Illegal data address"), "exception missing");
    }
    auto badEcho = framed(QByteArray::fromHex("010500010000"));
    check(!ModbusRtuCodec::decode(request(5), badEcho, summary, error) && error.contains("does not match"), "bad write echo accepted");
    check(!ModbusRtuCodec::decode(request(3), framed(QByteArray::fromHex("010302000aff00")), summary, error), "bad byte count accepted");
    check(!ModbusRtuCodec::decode(request(1), framed(QByteArray::fromHex("01010181")), summary, error), "invalid bit padding accepted");
    for (int function : {1,2,3,4,5,6,15,16}) {
        auto r = request(function);
        r.count = function <= 2 ? 2000 : function <= 4 ? 125 : function <= 6 ? 1 : function == 15 ? 1968 : 123;
        if (function > 4) r.values.fill(function == 5 || function == 15 ? 1 : 65535, r.count);
        check(!ModbusRtuCodec::encode(r, error).isEmpty(), "maximum legal quantity rejected");
        ++r.count;
        check(ModbusRtuCodec::encode(r, error).isEmpty(), "quantity overflow accepted");
    }
    for (int kind = 0; kind < 8; ++kind) {
        auto r = request(3);
        if (kind == 0) r.slave = 0;
        if (kind == 1) r.slave = 248;
        if (kind == 2) r.function = 7;
        if (kind == 3) r.address = -1;
        if (kind == 4) r.address = 65535;
        if (kind == 5) r.count = 0;
        if (kind == 6) r.values = {1};
        if (kind == 7) { r = request(5); r.values = {2}; }
        check(ModbusRtuCodec::encode(r, error).isEmpty(), "invalid request accepted");
    }
    QVector<quint16> values;
    check(ModbusRtuCodec::parseValues("10, 0x1234\n65535", values, error) && values == QVector<quint16>{10,0x1234,65535}, "write value parsing wrong");
    for (const auto &text : {"-1", "65536", "0xGG", "1.5", "1 garbage", "4294967296"})
        check(!ModbusRtuCodec::parseValues(text, values, error), "malformed write value accepted");
}

void exchangeTests() {
    for (int function : {1,2,3,4,5,6,15,16}) for (int split : {1,2,3,256}) {
        auto s = std::make_shared<State>(); s->response = reply(function); s->chunkSize = split;
        s->pending = "old bytes";
        Transport transport(s);
        const auto result = exchangeModbusRtu(transport, request(function), 200, 2);
        check(result.succeeded() && result.rx == s->response && result.tx == s->sent, qPrintable(result.error));
        check(s->timeout == 777 && !s->wrongThread, "timeout/owner not preserved");
    }
    {
        auto delayed = std::make_shared<State>(); delayed->gapInResponse = true; delayed->chunkSize = 2;
        Transport transport(delayed);
        int txEvents = 0;
        auto result = exchangeModbusRtu(transport, request(3), 200, 2, {}, [&](const QByteArray &tx) {
            check(delayed->writes == 0 && tx.toHex() == "010300000002c40b", "TX not published when sending"); ++txEvents;
        });
        check(result.succeeded() && txEvents == 1 && result.rx == delayed->response, "USB delivery gap falsely truncated a valid reply");
    }
    for (const QString mode : {"crc", "timeout", "truncated", "extra", "late_extra", "read", "write", "partial_write", "read_throw", "write_throw", "count", "clear", "late", "late_reply"}) {
        auto s = std::make_shared<State>();
        if (mode == "crc") s->response.back() ^= 1;
        if (mode == "timeout") s->response.clear();
        if (mode == "truncated") s->response.chop(1);
        if (mode == "late_reply") s->replyDelayMs = 110;
        if (mode == "extra") s->response.append('\n');
        if (mode == "late_extra") s->afterResponse = "x";
        if (mode == "read") s->readError = true;
        if (mode == "write") s->writeCount = -1;
        if (mode == "partial_write") s->writeCount = 3;
        if (mode == "read_throw") s->throwRead = true;
        if (mode == "write_throw") s->throwWrite = true;
        if (mode == "count") s->wrongCount = true;
        if (mode == "clear") s->clearOk = false;
        Transport transport(s);
        QElapsedTimer elapsed; elapsed.start();
        const auto result = exchangeModbusRtu(transport, request(3), mode == "late" ? 1 : 80, 2);
        check(!result.succeeded() && s->timeout == 777 && elapsed.elapsed() < 250, qPrintable("failure/timeout lost: " + mode));
        if (mode == "clear" || mode == "late") check(!s->writes, "invalid preparation transmitted");
        if (mode == "crc" || mode == "extra" || mode == "late_extra") check(!result.rx.isEmpty(), "bad RX diagnostics discarded");
    }
    auto s = std::make_shared<State>(); s->chunkSize = 1;
    Transport transport(s);
    auto cancelled = exchangeModbusRtu(transport, request(3), 100, 2, [] { return false; });
    check(!cancelled.succeeded() && !s->writes, "cancelled before send still wrote");
    cancelled = exchangeModbusRtu(transport, request(3), 100, 2, [&] { return s->readsAfterWrite < 2; });
    check(!cancelled.succeeded() && !cancelled.rx.isEmpty() && s->timeout == 777, "mid-read cancel ignored");
}

void viewModelTests() {
    auto s = std::make_shared<State>();
    CommandConsoleModel model;
    CommandConsoleViewModel vm(&model, [s](const QString &) { return std::make_unique<Transport>(s); });
    int factoryEvidence = 0;
    int txEvents = 0;
    QObject::connect(&vm, &CommandConsoleViewModel::modbusTransmitting, [&](const QByteArray &) {
        check(vm.isOperationPending(), "TX appeared after completion"); ++txEvents;
    });
    QObject::connect(&vm, &CommandConsoleViewModel::modbusTraffic, [&](const auto &tx, const auto &rx) {
        check(!tx.isEmpty() && !rx.isEmpty(), "TX/RX signal empty"); ++factoryEvidence;
    });
    check(vm.setMode(ConsoleMode::ModbusRtu), "mode switch failed");
    for (const QString address : {"MODBUS::127.0.0.1:502::SLAVE:1", "MODBUS::COM3:9600:7:N:1::SLAVE:1", "MODBUS::COM3:9600:8:N:1::SLAVE:0", "garbage"}) {
        vm.connectToAddress(address);
        check(!vm.isControlActive() && !vm.isOperationPending(), "invalid RTU address connected");
    }
    const QString address = "MODBUS::COM3:9600:8:N:1::SLAVE:1";
    vm.setControlAllowed(false); vm.connectToAddress(address);
    check(!vm.isControlActive(), "RTU bypassed cross-page lock");
    vm.setControlAllowed(true); vm.connectToAddress(address); settle(vm);
    check(vm.isConnected() && !vm.setMode(ConsoleMode::Scpi), "connected mode switch allowed");
    vm.setControlAllowed(false); vm.sendModbusRequest(1,3,0,2,"");
    check(!vm.isOperationPending() && !s->writes, "RTU send bypassed lock"); vm.setControlAllowed(true);
    vm.sendIDN(); check(s->writes == 0, "SCPI sent through RTU mode");
    vm.sendModbusRequest(1, 5, 0, 1, "2"); check(s->writes == 0 && !vm.isOperationPending(), "invalid write hit hardware");
    vm.sendModbusRequest(1, 3, 0, 2, "");
    vm.sendModbusRequest(1, 3, 0, 2, ""); settle(vm);
    check(s->writes == 1 && txEvents == 1 && factoryEvidence == 1 && model.fullHistory().first().success, "concurrent request or successful history wrong");
    check(model.commandHistory().isEmpty(), "RTU bytes contaminated SCPI history");
    s->response.back() ^= 1;
    vm.sendModbusRequest(1, 3, 0, 2, ""); settle(vm);
    check(!vm.isConnected() && !vm.isControlActive() && !model.fullHistory().first().success &&
          model.fullHistory().first().response.contains("CRC"), "CRC failure left transport or lock active");
    QString xml; QXmlStreamWriter out(&xml); model.writeXml(out);
    CommandConsoleModel restored; QXmlStreamReader in(xml); in.readNextStartElement(); restored.loadXml(in);
    check(!in.hasError() && restored.fullHistory().first().response == model.fullHistory().first().response, "RTU diagnostic XML roundtrip failed");
    s->response = reply(3);
    vm.connectToAddress(address); settle(vm);
    vm.sendModbusRequest(1,3,0,2,""); settle(vm);
    check(vm.isConnected() && model.fullHistory().first().success && !s->wrongThread, "reconnect did not recover");
    vm.disconnect(); settle(vm);
    check(vm.setMode(ConsoleMode::Scpi), "return to SCPI failed");
    s->response = "SCPI restored\n";
    vm.connectToAddress("offline-scpi"); settle(vm); vm.sendIDN(); settle(vm);
    check(s->sent == "*IDN?\n" && model.fullHistory().first().response == "SCPI restored", "SCPI behavior changed after switching");
    vm.disconnect(); settle(vm);
    vm.setMode(ConsoleMode::ModbusRtu); vm.connectToAddress(address); settle(vm);
    s->response.clear(); vm.setTimeout(60000);
    // A 60-second response timeout must still allow prompt disconnect and keep the lock until completion.
    QObject::disconnect(&vm, &CommandConsoleViewModel::modbusTraffic, nullptr, nullptr);
    QElapsedTimer elapsed; elapsed.start();
    vm.sendModbusRequest(1,3,0,2,"");
    QTimer::singleShot(20, &vm, [&] { vm.disconnect(); check(vm.isControlActive(), "cancel released lock before completion"); });
    settle(vm);
    check(elapsed.elapsed() < 500 && !vm.isConnected() && !model.fullHistory().first().success, "RTU cancel blocked on full deadline");
    auto pending = std::make_shared<State>(); pending->response.clear();
    auto *dying = new CommandConsoleViewModel(&model, [pending](const QString &) { return std::make_unique<Transport>(pending); });
    dying->setMode(ConsoleMode::ModbusRtu); dying->connectToAddress(address); settle(*dying);
    const auto historyCount = model.fullHistory().size();
    QPointer<CommandConsoleViewModel> alive(dying);
    dying->sendModbusRequest(1,3,0,2,"");
    QTimer::singleShot(20, dying, [dying] { delete dying; });
    elapsed.restart();
    while ((alive || !pending->destroyed.load()) && elapsed.elapsed() < 1000) { QApplication::processEvents(); QThread::msleep(1); }
    check(!alive && pending->destroyed && !pending->wrongThread && historyCount == model.fullHistory().size(), "RTU owner deletion leaked or delivered stale result");
}

void widgetTests() {
    auto s = std::make_shared<State>(); s->response = reply(1);
    CommandConsoleModel model;
    CommandConsoleViewModel vm(&model, [s](const QString &) { return std::make_unique<Transport>(s); });
    CommandConsolePage page(&vm); page.resize(850,650); page.show(); QApplication::processEvents();
    auto *mode = page.findChild<QComboBox*>("consoleMode");
    auto *editor = page.findChild<QPlainTextEdit*>(); editor->setPlainText("*IDN?");
    mode->setCurrentIndex(1); QApplication::processEvents();
    auto *send = page.findChild<QPushButton*>("modbusSend");
    auto *function = page.findChild<QComboBox*>("modbusFunction");
    auto *quantity = page.findChild<QSpinBox*>("modbusQuantity");
    auto *values = page.findChild<QLineEdit*>("modbusValues");
    auto *address = page.findChild<QComboBox*>("consoleAddress");
    check(!editor->isVisible() && send->isVisible() && !send->isEnabled(), "mode panes or offline Send incorrect");
    function->setCurrentIndex(function->findData(5));
    check(quantity->value() == 1 && !quantity->isEnabled() && values->isEnabled(), "single coil controls wrong");
    function->setCurrentIndex(function->findData(1)); quantity->setValue(2);
    address->setCurrentText("MODBUS::COM3:9600:8:N:1::SLAVE:1");
    QMetaObject::invokeMethod(&page,"onConnectClicked",Qt::DirectConnection); settle(vm);
    check(!mode->isEnabled() && send->isEnabled(), "connection mode lock wrong");
    send->click(); settle(vm);
    auto *log = page.findChild<QTextEdit*>("consoleLog");
    check(log->toPlainText().contains("[TX Hex]") && log->toPlainText().contains("[RX Hex]") &&
          log->toPlainText().contains("[Parsed]\n0x0000 = 1\n0x0001 = 0"), "hex or decoded response missing from UI");
    const auto writes = s->writes;
    auto *shortcut = page.findChild<QShortcut*>("modbusSendShortcut");
    check(shortcut && shortcut->key() == QKeySequence("Ctrl+Return"), "RTU shortcut missing");
    QMetaObject::invokeMethod(shortcut, "activated", Qt::DirectConnection); settle(vm);
    check(s->writes == writes + 1, "RTU shortcut did not dispatch");
    page.grab().save(TestEvidence::path("commandconsole/modbus.png"));
    vm.disconnect(); settle(vm); mode->setCurrentIndex(0); QApplication::processEvents();
    check(editor->isVisible() && editor->toPlainText() == "*IDN?", "mode switch lost SCPI draft");
    page.grab().save(TestEvidence::path("commandconsole/scpi.png"));
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
    app.setFont(QFont("Microsoft JhengHei", 9));
    try {
        const auto mode = app.arguments().value(1);
        if (mode == "codec") codecTests();
        else if (mode == "exchange") exchangeTests();
        else if (mode == "viewmodel") viewModelTests();
        else if (mode == "widget") widgetTests();
        else throw std::runtime_error("unknown test mode");
        std::cout << "PASS " << mode.toStdString() << '\n';
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
