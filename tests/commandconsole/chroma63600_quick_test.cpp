#include "../../src/views/commandconsole/chroma63600quickdialog.h"
#include <QApplication>
#include <QLocale>
#include <QFontDatabase>
#include <iostream>
#include <limits>
#include <stdexcept>
void require(bool ok) { if (!ok) throw std::runtime_error("63600 quick command check failed"); }
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        using namespace Chroma63600Commands;
        QLocale::setDefault(QLocale(QLocale::German));
        require(build(3, SlopeMode::Static, 0.5, 1.25, 12) ==
            "CHAN 3\nCURR:STAT:RISE 0.5\nCURR:STAT:FALL 1.25\nCONF:VOLT:ON 12");
        require(build(9, SlopeMode::Dynamic, 2, 3, 0) ==
            "CHAN 9\nCURR:DYN:RISE 2\nCURR:DYN:FALL 3\nCONF:VOLT:ON 0");
        require(build(10, SlopeMode::Both, 1, 2, 3).split('\n').size() == 6);
        int failures = 0;
        for (int ch : {0,11}) { try { build(ch,SlopeMode::Both,1,1,0); } catch (const std::invalid_argument &) { ++failures; } }
        for (double value : {0.0,-1.0,std::numeric_limits<double>::infinity()}) {
            try { build(1,SlopeMode::Static,value,1,0); } catch (const std::invalid_argument &) { ++failures; }
        }
        require(failures == 5);
        Chroma63600QuickDialog dialog;
        dialog.findChild<QSpinBox*>("channel")->setValue(5);
        dialog.findChild<QComboBox*>("slopeMode")->setCurrentIndex(2);
        dialog.findChild<QDoubleSpinBox*>("von")->setValue(24);
        require(dialog.commands() == "CHAN 5\nCURR:STAT:RISE 1\nCURR:STAT:FALL 1\nCURR:DYN:RISE 1\nCURR:DYN:FALL 1\nCONF:VOLT:ON 24");
        dialog.reject(); require(dialog.result() == QDialog::Rejected);
        if (argc > 1) {
            int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
            const auto families = QFontDatabase::applicationFontFamilies(font);
            if (!families.isEmpty()) app.setFont(QFont(families.first(), 10));
            dialog.show(); app.processEvents(); dialog.grab().save(QString::fromLocal8Bit(argv[1]));
        }
        std::cout << "PASS: static/dynamic/both commands, units, locale, validation and dialog fields\n";
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
