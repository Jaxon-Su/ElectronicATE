// chroma63600.cpp
#include "chroma63600.h"
#include <QDebug>
#include <QSet>
#include "chromaload63600spec.h"
#include <optional>
#include <algorithm>

Chroma63600::~Chroma63600() { disconnect(); }

// ==================== DCLoad Override ====================

void Chroma63600::setLoadOn()
{
    sendCommandWithLog(QString("LOAD ON"), "[Chroma63600]");
}

void Chroma63600::setLoadOff()
{
    sendCommandWithLog(QString("LOAD OFF"), "[Chroma63600]");
}

void Chroma63600::setChannel(int channel)
{
    QString cmd = QString("CHAN %1").arg(channel);
    write(cmd);
}

void Chroma63600::setLoadMode(const QString& mode)
{
    static const QSet<QString> validModes = {
        "CCL",  "CCM",  "CCH",
        "CCDL", "CCDM", "CCDH",
        "CRL",  "CRM",  "CRH",
        "CVL",  "CVM",  "CVH",
        "CPL",  "CPM",  "CPH",
        "CZ"
    };

    if (!validModes.contains(mode)) {
        qWarning() << "[Chroma63600] setLoadMode: Invalid mode:" << mode;
        return;
    }

    QString cmd = QString("MODE %1").arg(mode);
    write(cmd);
}

void Chroma63600::setVon(double von)
{
    QString cmd = QString("CONF:VOLT:ON %1").arg(von);
    write(cmd);
}

void Chroma63600::setStaticRiseSlope(double slope)
{
    QString cmd = QString("CURR:STAT:RISE %1").arg(slope);
    write(cmd);
}

void Chroma63600::setStaticFallSlope(double slope)
{
    QString cmd = QString("CURR:STAT:FALL %1").arg(slope);
    write(cmd);
}

void Chroma63600::setDynamicRiseSlope(double slope)
{
    QString cmd = QString("CURR:DYN:RISE %1").arg(slope);
    write(cmd);
}

void Chroma63600::setDynamicFallSlope(double slope)
{
    QString cmd = QString("CURR:DYN:FALL %1").arg(slope);
    write(cmd);
}

void Chroma63600::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    QString modeStr = selectOptimalLoadMode63600(m_model, maxCurrent, voltage);
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validStaticModes = {"CCL", "CCM", "CCH"};
        modeStr = validStaticModes.contains(requestedMode) ? requestedMode : QStringLiteral("CCH");
    }
    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;
        if (i >= param.levels.size()) continue;

        double value    = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:STAT:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma63600] setStaticCurrent:" + segName);
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma63600] Only L1/L2 supported, ignore L3+";
    }
}

void Chroma63600::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    QString baseMode = selectOptimalLoadMode63600(m_model, maxCurrent, voltage);

    QString modeStr;
    if (baseMode == "CCH") {
        modeStr = "CCDH";
    } else if (baseMode == "CCM") {
        modeStr = "CCDM";
    } else {
        modeStr = "CCDL";
    }
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validDynamicModes = {"CCDL", "CCDM", "CCDH"};
        modeStr = validDynamicModes.contains(requestedMode) ? requestedMode : QStringLiteral("CCDH");
    }

    qDebug() << "[Chroma63600][setDynamicCurrent] baseMode=" << baseMode
             << "modeStr=" << modeStr;

    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    int segs = getNumSegments();
    for (int i = 0; i < segs && i < param.levels.size(); ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;

        double value    = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:DYN:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma63600] setDynamicCurrent:" + segName);
    }

    if (!param.timings.isEmpty()) {
        if (param.timings.size() >= 1) {
            QString cmd = QString("CURR:DYN:T1 %1").arg(param.timings[0], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma63600] setDynamicCurrent:T1");
        }
        if (param.timings.size() >= 2) {
            QString cmd = QString("CURR:DYN:T2 %1").arg(param.timings[1], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma63600] setDynamicCurrent:T2");
        }
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma63600] Only L1/L2 supported for dynamic mode, ignore L3+";
    }
}

QString Chroma63600::model() const
{
    return "63600";
}

QString Chroma63600::vendor() const
{
    return "Chroma";
}

// ==================== 63600 特有功能 ====================

void Chroma63600::setCurrentRange(const QString& range)
{
    static const QSet<QString> validRanges = {"L", "M", "H"};
    if (!validRanges.contains(range.toUpper())) {
        qWarning() << "[Chroma63600] setCurrentRange: Invalid range:" << range;
        return;
    }
    QString cmd = QString("CURR:RANG %1").arg(range.toUpper());
    write(cmd);
}

void Chroma63600::setVoltageRange(const QString& range)
{
    static const QSet<QString> validRanges = {"L", "M", "H"};
    if (!validRanges.contains(range.toUpper())) {
        qWarning() << "[Chroma63600] setVoltageRange: Invalid range:" << range;
        return;
    }
    QString cmd = QString("VOLT:RANG %1").arg(range.toUpper());
    write(cmd);
}

void Chroma63600::setPower(double power)
{
    QString cmd = QString("POW:STAT:L1 %1").arg(power);
    write(cmd);
}

// ==================== SYNC Dynamic ====================

void Chroma63600::setSyncType(int type)
{
    static const char* typeStr[] = {"NONE", "MASTER", "SLAVE"};
    if (type < 0 || type > 2) {
        qWarning() << "[Chroma63600] setSyncType: invalid type" << type;
        return;
    }

    qDebug() << "[Chroma63600] setSyncType"
             << "address=" << getaddress()
             << "realChannel=" << realChannel()
             << "syncChannelCmd=" << QString("SYNC:CHAN %1").arg(realChannel())
             << "syncTypeCmd=" << QString("SYNC:TYPE %1").arg(typeStr[type]);
    setSyncChannel(realChannel());
    sendCommandWithLog(QString("SYNC:TYPE %1").arg(typeStr[type]), "[Chroma63600]");
}

int Chroma63600::syncType()
{
    // 63600 manual documents SYNChronous:TYPE as a set command only.
    // Keep the software-side role deterministic instead of issuing SYNC:TYPE?.
    return defaultSyncType();
}

int Chroma63600::defaultSyncType() const
{
    return configuredSyncType();
}

void Chroma63600::setSyncChannel(int ch)
{
    // 63600 uses SYNC:CHAN to choose which module's sync role is edited.
    QString cmd = QString("SYNC:CHAN %1").arg(ch);
    qDebug() << "[Chroma63600] setSyncChannel"
             << "address=" << getaddress()
             << "realChannel=" << realChannel()
             << "cmd=" << cmd;
    sendCommandWithLog(cmd, "[Chroma63600]");
}

void Chroma63600::setSyncRun(bool on)
{
    // 只對 MASTER 呼叫，觸發全部 channel 同步啟動/停止
    QString cmd = QString("SYNC:RUN %1").arg(on ? "ON" : "OFF");
    sendCommandWithLog(cmd, "[Chroma63600]");
}
