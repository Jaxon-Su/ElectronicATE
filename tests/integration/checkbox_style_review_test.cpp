#include "../../src/views/testsequence/dialogs/stressdialogs/staticdialog.h"
#include "../../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.h"
#include "../../src/views/testsequence/dialogs/turnonoffdialogs/turnonoffdialog.h"
#include "../../src/views/testsequence/dialogs/turnonoffdialogs/shorttestdialog.h"
#include "../../src/views/testsequence/dialogs/brownoutdialog.h"
#include "../../src/views/testsequence/dialogs/oscilloscopedialogs/msoseries456writedialog.h"
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QStyleOptionButton>
#include <QFontDatabase>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); QApplication app(argc, argv);
    for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "msjh.ttc"})
        QFontDatabase::addApplicationFont(QString("C:/Windows/Fonts/") + font);
    try {
        QDir().mkpath("checkbox-review");
        QImage sheet(1120, 1680, QImage::Format_ARGB32); sheet.fill(Qt::white);
        QPainter painter(&sheet); painter.setFont(QFont("Segoe UI", 10));
        int row = 0, boxes = 0;
        auto inspect = [&](QDialog& dialog, QString name) {
            dialog.show(); app.processEvents();
            auto checks = dialog.findChildren<QCheckBox*>();
            require(!checks.isEmpty(), "dialog has no checkboxes");
            painter.setPen(Qt::black); painter.drawText(10, 24 + row * 95, name);
            int x = 10;
            // Every real checkbox is rendered in all four states, including hidden Advanced controls.
            // Compare its indicator to the resource pixels, independently of objectName/QSS text.
            for (auto* box : checks) {
                ++boxes;
                const bool wasChecked = box->isChecked(), wasEnabled = box->isEnabled();
                for (bool enabled : {true, false}) for (bool checked : {false, true}) {
                    box->setEnabled(enabled); box->setChecked(checked); box->ensurePolished();
                    QStyleOptionButton opt; opt.initFrom(box);
                    opt.rect = QRect(0, 0, 16, 16);
                    opt.state &= ~(QStyle::State_On | QStyle::State_Off);
                    opt.state |= checked ? QStyle::State_On : QStyle::State_Off;
                    QImage actual(16, 16, QImage::Format_ARGB32_Premultiplied); actual.fill(Qt::transparent);
                    QPainter p(&actual);
                    box->style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &opt, &p, box); p.end();
                    const QImage expected = QPixmap(checked ? ":/images/checkbox-checked.svg" : ":/images/checkbox-unchecked.svg")
                                                .toImage().convertToFormat(actual.format());
                    require(!expected.isNull() && actual == expected, "checkbox does not render the shared SVG indicator");
                }
                box->setChecked(wasChecked); box->setEnabled(wasEnabled);
            }
            // Compact visual sample of the first option with its label in each state.
            auto* sample = dialog.findChild<QCheckBox*>("captureEnabled");
            if (!sample) sample = checks.first();
            for (bool enabled : {true, false}) for (bool checked : {false, true}) {
                sample->setEnabled(enabled); sample->setChecked(checked); app.processEvents();
                auto pix = sample->grab();
                painter.drawPixmap(x, 30 + row * 95, pix);
                painter.drawText(x, 74 + row * 95, QString("%1 / %2").arg(enabled?"enabled":"disabled",checked?"checked":"unchecked"));
                x += 280;
            }
            dialog.grab().save("checkbox-review/" + name + ".png");
            ++row;
        };
        const InputConditionOptions inputs{{"AC 90V"}, {"DC 48V"}};
        const QStringList loads{"100%", "90%"};
        for (auto selection : {LoadConditionSelector::Selection::Single, LoadConditionSelector::Selection::Group}) {
            const QString suffix = selection == LoadConditionSelector::Selection::Single ? " single" : " group";
            StaticDialog stat(inputs, loads, {}, 1, nullptr, selection); inspect(stat, "Static" + suffix);
            DynamicDialog dyn(inputs, loads, {}, 1, nullptr, selection); inspect(dyn, "Dynamic" + suffix);
            TurnOnOffDialog on(TurnOnOffDialog::TurnOn, inputs, loads, {}, {}, 1, nullptr, selection); inspect(on, "Turn on" + suffix);
            TurnOnOffDialog off(TurnOnOffDialog::TurnOff, inputs, loads, {}, {}, 1, nullptr, selection); inspect(off, "Turn off" + suffix);
            ShortTestDialog before(ShortTestDialog::ShortThenTurnOn, inputs, loads, {}, {}, 1, nullptr, selection); inspect(before, "Short then on" + suffix);
            ShortTestDialog after(ShortTestDialog::TurnOnThenShort, inputs, loads, {}, {}, 1, nullptr, selection); inspect(after, "On then short" + suffix);
        }
        BrownOutDialog brown(inputs, loads, {}, 1); inspect(brown, "Brown Out");
        CaptureDialog capture({}, nullptr); inspect(capture, "Capture formats");
        MSOSeries456WriteDialog write(nullptr, "MSO44B", {}, 1, ""); inspect(write, "Write scope");
        painter.end(); require(sheet.save("checkbox-review/contact-sheet.png"), "cannot save UI evidence");
        std::cout << "PASS " << row << " dialogs, " << boxes << " checkboxes x 4 states\n";
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 1; }
}
