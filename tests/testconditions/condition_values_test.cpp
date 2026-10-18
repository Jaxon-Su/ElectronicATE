#include "conditionvalues.h"
#include "conditionpower.h"
#include <QCoreApplication>
#include <QLocale>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}
// Frozen pre-refactor execution parser: accepted values must remain identical.
bool legacy(QString text, QChar unit, double &value)
{
    text = text.trimmed();
    text.remove(QRegularExpression(R"(\s+)"));
    if (text.endsWith(unit, Qt::CaseInsensitive)) text.chop(1);
    bool ok = false;
    value = text.toDouble(&ok);
    return ok && std::isfinite(value) && value >= 0;
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QStringList values{"", " ", "0", "-0", "1.5", " 1.5 A ", "1.5a",
            "1 2 A", "1e-3", "+2", "-1", "nan", "inf", "1e999", "1,5", "2mA",
            "12V", "1A;LOAD ON", "1\nA", "1/2", ".5", "1."};
        for (auto language : {QLocale::English, QLocale::German}) {
            QLocale::setDefault(QLocale(language));
            for (QChar unit : {QChar('A'), QChar('V')}) {
                for (const auto &text : values) {
                    double oldValue = 0, value = 0;
                    const bool oldOk = legacy(text, unit, oldValue);
                    const bool ok = ConditionValues::nonNegativeWithUnit(text, unit, value);
                    check(ok == oldOk && (!ok || value == oldValue), "execution parser changed");
                }
            }
        }
        double voltage = 0, current = 0;
        check(ConditionValues::cvPair("12V/2a", voltage, current) && voltage == 12 && current == 2, "CV pair");
        for (const auto &text : {"/12V/2A", "12V//2A", "12V/2A/", "12V/", "-12V/2A", "12A/2A"})
            check(!ConditionValues::cvPair(text, voltage, current), "invalid CV accepted");
        check(ConditionValues::finiteNumber("-90", voltage), "signed phase rejected");
        check(!ConditionValues::finiteNumber("inf", voltage), "nonfinite accepted");
        check(ConditionValues::editorPattern("range").match("1~2").hasMatch(), "range grammar");
        check(!ConditionValues::editorPattern("range").match("1~~2").hasMatch(), "invalid range");
        check(ConditionValues::editorPattern("load").match("12V/2A").hasMatch(), "load grammar");
        LoadMetaRow meta; meta.modes = {"CV"};
        check(ConditionPower::load(meta, {"12V/2A"}, 1) == 24, "power changed");
        check(std::isnan(ConditionPower::load(meta, {"/12V/2A"}, 1)), "power accepts rejected pair");
        check(std::isnan(ConditionPower::load(meta, {"-12V/2A"}, 1)), "power accepts negative load");
        std::cout << "PASS condition rules and execution compatibility\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
