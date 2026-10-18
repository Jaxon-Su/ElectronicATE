#include "../../src/infrastructure/discovery/commscanprocess.h"
#include "../../src/viewmodels/instrumentsetup/commscanviewmodel.h"
#include "../../src/views/instrumentsetup/dialogs/commscandialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <cstdio>
#include <functional>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void waitUntil(const std::function<bool()> &condition, int timeout = 4000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout) {
        QApplication::processEvents();
        QThread::msleep(1);
    }
    check(condition(), "timed out waiting for asynchronous scanner");
}
void send(const QJsonObject &object)
{
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    std::fflush(stdout);
}
int helper()
{
    const auto mode = qgetenv("COMM_SCAN_TEST_MODE");
    if (mode == "hang") {
        QThread::sleep(30);
        return 0;
    }
    if (mode == "bad") {
        std::puts("bad JSON");
        std::fflush(stdout);
        return 0;
    }
    if (mode != "empty") {
        send({{"entry", CommScanEntry{"TCPIP", "TCPIP0::192.0.2.1::inst0::INSTR", {}, "VISA"}.toJson()}});
        send({{"entry", CommScanEntry{"TCPIP", "TCPIP0::192.0.2.1::INSTR", {}, "VISA"}.toJson()}});
        if (mode == "reverse")
            send({{"entry", CommScanEntry{"Serial", "COM99", {}, "VISA", "ASRL7::INSTR"}.toJson()}});
        send({{"entry", CommScanEntry{"Serial", "COM99", "Simulated USB Serial Port", "System"}.toJson()}});
        send({{"entry", CommScanEntry{"Serial", "COM99", {}, "VISA", "ASRL7::INSTR"}.toJson()}});
        if (mode == "unmapped")
            send({{"entry", CommScanEntry{"ASRL", "ASRL99::INSTR", {}, "VISA"}.toJson()}});
    }
    if (mode == "partial") QThread::sleep(30);
    if (mode == "warning") send({{"warning", "Simulated VISA discovery failure."}});
    send({{"done", true}});
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == "--scan-comm-helper") {
        QCoreApplication app(argc, argv);
        return helper();
    }
    QApplication app(argc, argv);
    const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    const auto families = QFontDatabase::applicationFontFamilies(font);
    if (!families.isEmpty()) app.setFont(QFont(families.first(), 9));
    try {
        const auto program = QCoreApplication::applicationFilePath();
        CommScanProcess process(program);
        int finishes = 0;
        QString error;
        QObject::connect(&process, &CommScanProcess::finished, [&](const QString &value) { error = value; ++finishes; });
        qputenv("COMM_SCAN_TEST_MODE", "hang");
        process.start(100);
        waitUntil([&] { return finishes == 1; });
        check(error.contains("timed out") && !process.isRunning(), "hung helper was not terminated");
        qputenv("COMM_SCAN_TEST_MODE", "bad");
        process.start(1000);
        waitUntil([&] { return finishes == 2; });
        check(!error.isEmpty(), "invalid process output accepted");
        qputenv("COMM_SCAN_TEST_MODE", "hang");
        process.start(30000);
        process.stop();
        waitUntil([&] { return finishes == 3; });
        check(error == "Stopped.", "stop did not finish promptly");
        QElapsedTimer destructorTime;
        destructorTime.start();
        {
            CommScanProcess destroyed(program);
            destroyed.start(30000);
        }
        check(destructorTime.elapsed() < 1500, "destroying scanner waits for native operation");
        {
            CommScanProcess missing("C:/nonexistent-ElectronicATE-scanner.exe");
            CommScanViewModel missingVm(&missing);
            QString status;
            QObject::connect(&missingVm, &CommScanViewModel::statusChanged, [&](const QString &value) { status = value; });
            missingVm.start();
            waitUntil([&] { return !missingVm.isRunning(); });
            check(status.contains("Cannot start scanner"), "failed helper left UI locked or hid error");
        }
        qputenv("COMM_SCAN_TEST_MODE", "discover");
        CommScanViewModel vm(&process);
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(vm.entries().size() == 2 && vm.entries()[0].source == "VISA" &&
                  vm.entries()[1].address == "COM99", "enumerated resources or deduplication lost");
        check(vm.entries()[1].source == "System / VISA" &&
                  vm.entries()[1].visaAddress == "ASRL7::INSTR" &&
                  vm.entries()[1].description == "Simulated USB Serial Port",
              "serial merge lost the VISA alias, source or system description");
        qputenv("COMM_SCAN_TEST_MODE", "reverse");
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(vm.entries().size() == 2 && vm.entries()[1].source == "System / VISA" &&
                  vm.entries()[1].description == "Simulated USB Serial Port" &&
                  vm.entries()[1].visaAddress == "ASRL7::INSTR",
              "serial merge depends on arrival order or duplicates sources");
        qputenv("COMM_SCAN_TEST_MODE", "unmapped");
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(vm.entries().size() == 3 && vm.entries()[2].address == "ASRL99::INSTR",
              "an unmapped VISA resource was merged using its port number");
        qputenv("COMM_SCAN_TEST_MODE", "discover");
        CommScanDialog dialog(&vm);
        dialog.show();
        waitUntil([&] { return vm.isRunning(); });
        waitUntil([&] { return !vm.isRunning(); });
        auto *table = dialog.findChild<QTableWidget *>("commScanResults");
        check(table && table->rowCount() == 2 && table->columnCount() == 4 &&
                  table->item(0, 2)->text() == "VISA", "resource UI did not update");
        check(table->item(1, 1)->text() == "COM99 (ASRL7::INSTR)" &&
                  table->item(1, 2)->text() == "System / VISA" &&
                  dialog.findChild<QLabel *>("commScanStatus")->text().contains("2 resources"),
              "merged resources were not displayed or counted as one entry");
        check(dialog.findChildren<QCheckBox *>().isEmpty(), "device verification option remains");
        check(dialog.findChild<QPushButton *>("scanCommStart")->isEnabled() &&
                  !dialog.findChild<QPushButton *>("scanCommStop")->isEnabled(), "scan controls stayed locked");
        table->selectRow(1);
        for (auto *button : dialog.findChildren<QPushButton *>())
            if (button->text() == "Copy Address") button->click();
        check(QApplication::clipboard()->text() == "COM99", "Copy Address copied the wrong value");
        if (argc > 1) dialog.grab().save(QString::fromLocal8Bit(argv[1]));
        qputenv("COMM_SCAN_TEST_MODE", "warning");
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(dialog.findChild<QLabel *>("commScanStatus")->text().contains("Simulated VISA"),
              "partial discovery error hidden");
        qputenv("COMM_SCAN_TEST_MODE", "empty");
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(vm.entries().isEmpty() && table->rowCount() == 0, "rescan retained removed resources");
        qputenv("COMM_SCAN_TEST_MODE", "partial");
        vm.start();
        waitUntil([&] { return vm.entries().size() == 2; });
        dialog.reject();
        waitUntil([&] { return !vm.isRunning(); });
        check(!dialog.isVisible() && !process.isRunning() && vm.entries().size() == 2,
              "closing scan dialog left helper running or lost partial results");
        qputenv("COMM_SCAN_TEST_MODE", "discover");
        vm.start();
        waitUntil([&] { return !vm.isRunning(); });
        check(vm.entries().size() == 2, "cannot scan again after cancellation");
        std::cout << "PASS: resource listing, UI, copy, rescan, errors, cancellation and timeouts\n";
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
