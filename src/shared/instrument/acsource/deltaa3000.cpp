// deltaa3000.cpp 片段
#include "deltaa3000.h"
#include <QDebug>

DeltaA3000::DeltaA3000(ICommunication* comm) : ACSource(comm) {}
DeltaA3000::~DeltaA3000() { disconnect(); }

QString DeltaA3000::model() const { return "DE-A3000AB"; }
QString DeltaA3000::vendor() const { return "Delta"; }

void DeltaA3000::setVoltage(double v) {
    // qDebug() << QString("SOURce:VOLTage:AC %1").arg(v, 0, 'f', 3);
    sendCommandWithLog(QString("SOURce:VOLTage:AC %1").arg(v, 0, 'f', 3), "[DeltaA3000]");
}
void DeltaA3000::setFrequency(double f) {
     // DeltaA3000 Frequency最低到30
     // qDebug() << QString("SOURce:FREQuency %1").arg(f, 0, 'f', 3);
    sendCommandWithLog(QString("SOURce:FREQuency %1").arg(f, 0, 'f', 3), "[DeltaA3000]");
}

void DeltaA3000::setPhaseOn(double p) {
    // qDebug() << QString("SOURce:PHASe:ON %1").arg(p, 0, 'f', 3);
    sendCommandWithLog(QString("SOURce:PHASe:ON %1").arg(p, 0, 'f', 3), "[DeltaA3000]");
}

void DeltaA3000::setPhaseOff(double p) {
     //SOURce:PHASe:OFF value
    // qDebug() << QString("SOURce:PHASe:OFF %1").arg(p, 0, 'f', 3);
    sendCommandWithLog(QString("SOURce:PHASe:OFF %1"
                               "").arg(p, 0, 'f', 3), "[DeltaA3000]");
}

void DeltaA3000::setPowerOn() {
    //OUTPut ON
    // qDebug() << QString("OUTPut ON");
    sendCommandWithLog(QString("OUTPut ON"), "[DeltaA3000]");
}

void DeltaA3000::setPowerOff() {
    //OUTPut OFF
    // qDebug() << QString("OUTPut OFF");
    sendCommandWithLog(QString("OUTPut OFF"), "[DeltaA3000]");
}

double DeltaA3000::measureVoltage() {
    double v = 0.0;
    queryDouble("MEAS:VOLT:AC?", v);
    return v;
}
double DeltaA3000::measureCurrent() {
    double i = 0.0;
    queryDouble("MEAS:CURR:AC?", i);
    return i;
}
double DeltaA3000::realPower() {
    double p = 0.0;
    queryDouble("MEAS:POWer:REAL?", p);
    return p;
}
double DeltaA3000::reactivePower() {
    double q = 0.0;
    queryDouble("MEAS:POWer:REACtive?", q);
    return q;
}
double DeltaA3000::apparentPower() {
    double s = 0.0;
    queryDouble("MEAS:POWer:APParent?", s);
    return s;
}
double DeltaA3000::powerPfactor() {
    double pf = 0.0;
    queryDouble("MEAS:POWer:PFACtor?", pf);
    return pf;
}
double DeltaA3000::freQuency() {
    double f = 0.0;
    queryDouble("MEAS:FREQuency?", f);
    return f;
}
