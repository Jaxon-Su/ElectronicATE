#include "chroma63200a.h"
#include "chromaload63200aspec.h"
#include <QDebug>
#include <QSet>
#include <algorithm>

Chroma63200A::~Chroma63200A() { disconnect(); }

// ==================== DCLoad Override ====================

void Chroma63200A::setChannel(int channel)
{
    QString cmd = QString("CHAN %1").arg(channel);
    write(cmd);
}

void Chroma63200A::setLoadOn()
{
    sendCommandWithLog("LOAD ON", "[Chroma63200A]");
}

void Chroma63200A::setLoadOff()
{
    sendCommandWithLog("LOAD OFF", "[Chroma63200A]");
}

void Chroma63200A::setSyncType(int type)
{
    static const char* typeStr[] = {"NONE", "MASTER", "SLAVE"};

    if (type < 0 || type > 2) {
        qWarning() << "[Chroma63200A] setSyncType: invalid type" << type;
        return;
    }

    sendCommandWithLog(QString("SYNC:TYPE %1").arg(typeStr[type]), "[Chroma63200A]");
}

int Chroma63200A::syncType()
{
    QString result;
    if (!queryString("SYNC:TYPE?", result))
        return -1;

    const QString value = result.trimmed().toUpper();
    if (value.contains("MASTER") || value == "1")
        return 1;
    if (value.contains("SLAVE") || value == "2")
        return 2;
    if (value.contains("NONE") || value == "0")
        return 0;
    return -1;
}

void Chroma63200A::setSyncChannel(int ch)
{
    sendCommandWithLog(QString("SYNC:CHAN %1").arg(ch), "[Chroma63200A]");
}

void Chroma63200A::setSyncRun(bool on)
{
    sendCommandWithLog(QString("SYNC:RUN %1").arg(on ? "ON" : "OFF"), "[Chroma63200A]");
}

void Chroma63200A::setLoadMode(const QString& mode)
{
    // 63200A 支援的完整模式集（手冊 p.4-8, MODE 子系統）
    static const QSet<QString> validModes = {
        "CCL",  "CCM",  "CCH",      // Constant Current Static (Low/Mid/High)
        "CCDL", "CCDM", "CCDH",     // Constant Current Dynamic
        "CRL",  "CRM",  "CRH",      // Constant Resistance Static
        "CRDL", "CRDM", "CRDH",     // Constant Resistance Dynamic
        "CVL",  "CVM",  "CVH",      // Constant Voltage
        "CPL",  "CPM",  "CPH",      // Constant Power
        "CZL",  "CZM",  "CZH",      // Constant Impedance
        "CVCC", "CRCC", "CVCR",     // Combined modes
        "BATL", "BATM", "BATH",     // Battery Discharge
        "SWDL", "SWDM", "SWDH",     // Sine Wave Dynamic
        "OCPL", "OCPM", "OCPH",     // OCP test
        "OPPL", "OPPM", "OPPH",     // OPP test
        "CCSL", "CCSM", "CCSH",     // CCS (constant current sweep)
        "MPPTL","MPPTM","MPPTH",    // MPPT
        "UDWL", "UDWM", "UDWH",     // User Defined Waveform
        "EXTL", "EXTM", "EXTH",     // External waveform
        "AUTO", "PROG"
    };

    if (!validModes.contains(mode)) {
        qWarning() << "[Chroma63200A] setLoadMode: Invalid mode:" << mode;
        return;
    }

    QString cmd = QString("MODE %1").arg(mode);
    write(cmd);
}

void Chroma63200A::setVon(double von)
{
    // 手冊 p.4-11：CONF:VOLT:ON <NRf+>
    QString cmd = QString("CONF:VOLT:ON %1").arg(von);
    write(cmd);
}

void Chroma63200A::setStaticRiseSlope(double slope)
{
    // 手冊 p.4-20：CURR:STAT:RISE <NRf+>  單位 A/μs
    QString cmd = QString("CURR:STAT:RISE %1").arg(slope);
    write(cmd);
}

void Chroma63200A::setStaticFallSlope(double slope)
{
    QString cmd = QString("CURR:STAT:FALL %1").arg(slope);
    write(cmd);
}

void Chroma63200A::setDynamicRiseSlope(double slope)
{
    // 手冊 p.4-23：CURR:DYN:RISE <NRf+>
    QString cmd = QString("CURR:DYN:RISE %1").arg(slope);
    write(cmd);
}

void Chroma63200A::setDynamicFallSlope(double slope)
{
    QString cmd = QString("CURR:DYN:FALL %1").arg(slope);
    write(cmd);
}

void Chroma63200A::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    // 取所有 levels 的最大值來選擇檔位（避免 L2 電流大於 L1 時選錯範圍）
    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    QString modeStr = selectOptimalLoadMode63200A(m_subModel, maxCurrent, voltage);
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validStaticModes = {"CCL", "CCM", "CCH"};
        modeStr = validStaticModes.contains(requestedMode) ? requestedMode : QStringLiteral("CCH");
    }
    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    // 設定每個 segment 的電流 (L1, L2)
    int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;
        if (i >= param.levels.size()) continue;

        double   value   = param.levels[i];
        QString  segName = (i == 0) ? "L1" : "L2";
        // 手冊 p.4-20：CURR:STAT:L1 / CURR:STAT:L2
        QString  cmd     = QString("CURR:STAT:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma63200A] setStaticCurrent:" + segName);
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma63200A] Only L1/L2 supported, ignore L3+";
    }
}

void Chroma63200A::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    // 先找靜態最佳模式，再轉為對應動態模式
    // 63200A 動態對應：CCL → CCDL，CCM → CCDM，CCH → CCDH
    QString baseMode = selectOptimalLoadMode63200A(m_subModel, maxCurrent, voltage);
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

    qDebug() << "[Chroma63200A][setDynamicCurrent] baseMode=" << baseMode
             << "modeStr=" << modeStr;

    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    // 設定動態電流 L1 / L2
    // 手冊 p.4-21：CURR:DYN:L1 / CURR:DYN:L2
    int segs = getNumSegments();
    for (int i = 0; i < segs && i < param.levels.size(); ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;

        double  value   = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:DYN:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma63200A] setDynamicCurrent:" + segName);
    }

    // 設定動態時間參數 T1 / T2
    // 手冊 p.4-22：CURR:DYN:T1 / CURR:DYN:T2
    // 解析度 1μs → 格式化至小數點後 10 位以保留完整精度（參考 chroma63600.cpp 做法）
    if (!param.timings.isEmpty()) {
        if (param.timings.size() >= 1) {
            QString cmd = QString("CURR:DYN:T1 %1").arg(param.timings[0], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma63200A] setDynamicCurrent:T1");
        }
        if (param.timings.size() >= 2) {
            QString cmd = QString("CURR:DYN:T2 %1").arg(param.timings[1], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma63200A] setDynamicCurrent:T2");
        }
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma63200A] Only L1/L2 supported for dynamic mode, ignore L3+";
    }
}

void Chroma63200A::setCVVoltage(double voltage)
{
    const QString modeStr = selectOptimalCVMode63200A(m_subModel, voltage, 0.0);
    setLoadMode(modeStr);

    const int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        const QString segName = (i == 0) ? "L1" : "L2";
        sendCommandWithLog(
            QString("VOLT:STAT:%1 %2").arg(segName).arg(voltage, 0, 'f', 3),
            "[Chroma63200A] setCVVoltage:" + segName);
    }
}

void Chroma63200A::setCVCurrentLimit(double currentLimit)
{
    sendCommandWithLog(
        QString("VOLT:STAT:ILIM %1").arg(currentLimit, 0, 'f', 3),
        "[Chroma63200A] setCVCurrentLimit");
}

void Chroma63200A::setCVSettings(double voltage, double currentLimit)
{
    const QString modeStr = selectOptimalCVMode63200A(m_subModel, voltage, currentLimit);
    setLoadMode(modeStr);

    const int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        const QString segName = (i == 0) ? "L1" : "L2";
        sendCommandWithLog(
            QString("VOLT:STAT:%1 %2").arg(segName).arg(voltage, 0, 'f', 3),
            "[Chroma63200A] setCVSettings:Voltage:" + segName);
    }

    sendCommandWithLog(
        QString("VOLT:STAT:ILIM %1").arg(currentLimit, 0, 'f', 3),
        "[Chroma63200A] setCVSettings:Ilimit");
}

void Chroma63200A::setCVSettings(double voltage, double currentLimit, const QString& loadMode)
{
    QString modeStr = selectOptimalCVMode63200A(m_subModel, voltage, currentLimit);
    const QString requestedMode = loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validCVModes = {"CVL", "CVM", "CVH"};
        modeStr = validCVModes.contains(requestedMode) ? requestedMode : QStringLiteral("CVH");
    }

    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    const int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        const QString segName = (i == 0) ? "L1" : "L2";
        sendCommandWithLog(
            QString("VOLT:STAT:%1 %2").arg(segName).arg(voltage, 0, 'f', 3),
            "[Chroma63200A] setCVSettings:Voltage:" + segName);
    }

    sendCommandWithLog(
        QString("VOLT:STAT:ILIM %1").arg(currentLimit, 0, 'f', 3),
        "[Chroma63200A] setCVSettings:Ilimit");
}

QString Chroma63200A::model() const
{
    // 回傳完整子型號，例如 "63205A-150-500"
    return m_subModel;
}

QString Chroma63200A::vendor() const
{
    return "Chroma";
}
