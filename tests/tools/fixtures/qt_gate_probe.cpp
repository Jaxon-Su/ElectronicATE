#include <QtTest>

// Controlled infrastructure fixture: a QtTest skip normally exits successfully.
class GateProbe : public QObject
{
    Q_OBJECT
private slots:
    void passes() { QVERIFY(true); }
    void skips() { QSKIP("Intentional gate regression fixture"); }
};

QTEST_APPLESS_MAIN(GateProbe)
#include "qt_gate_probe.moc"
