#include "chroma6310.h"
#include <QDebug>
#include <QSet>
#include "chromaload6310spec.h"
#include <optional>
#include <algorithm>

Chroma6310::~Chroma6310() { disconnect(); }

// ==================== DCLoad Override ====================

void Chroma6310::setLoadOn()
{
    sendCommandWithLog(QString("LOAD ON"), "[Chroma6310]");
}

void Chroma6310::setLoadOff()
{
    sendCommandWithLog(QString("LOAD OFF"), "[Chroma6310]");
}

void Chroma6310::setChannel(int channel)
{
    QString cmd = QString("CHAN %1").arg(channel);
    write(cmd);
}

void Chroma6310::setLoadMode(const QString& mode)
{
    static const QSet<QString> validModes = {
        "CCL", "CCH", "CCDL", "CCDH", "CRL", "CRH", "CV"
    };
    if (!validModes.contains(mode)) {
        qWarning() << "[Chroma6310] setLoadMode: Invalid mode:" << mode;
        return;
    }
    QString cmd = QString("MODE %1").arg(mode);
    write(cmd);
}

void Chroma6310::setVon(double von)
{
    QString cmd = QString("CONF:VOLT:ON %1").arg(von);
    write(cmd);
}

void Chroma6310::setStaticRiseSlope(double slope)
{
    QString cmd = QString("CURR:STAT:RISE %1").arg(slope);
    write(cmd);
}

void Chroma6310::setStaticFallSlope(double slope)
{
    QString cmd = QString("CURR:STAT:FALL %1").arg(slope);
    write(cmd);
}

void Chroma6310::setDynamicRiseSlope(double slope)
{
    QString cmd = QString("CURR:DYN:RISE %1").arg(slope);
    write(cmd);
}

void Chroma6310::setDynamicFallSlope(double slope)
{
    QString cmd = QString("CURR:DYN:FALL %1").arg(slope);
    write(cmd);
}

void Chroma6310::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    // 修正：取所有 levels 的最大值來選擇檔位
    // 原本只用 levels[0]，若 L2 電流大於 L1，選出的檔位可能不足以容納 L2
    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    QString modeStr = selectOptimalLoadMode(m_model, maxCurrent, voltage);
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validStaticModes = {"CCL", "CCH"};
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

        double value   = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:STAT:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310] setStaticCurrent:" + segName);
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310] Only L1/L2 supported, ignore L3+";
    }
}

void Chroma6310::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    // 取所有 levels 的最大值來選擇檔位
    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    // 先選靜態模式，再轉換為對應動態模式
    QString baseMode = selectOptimalLoadMode(m_model, maxCurrent, voltage);

    // 6310 動態模式對應：CCL → CCDL，CCH → CCDH
    QString modeStr = (baseMode == "CCH") ? "CCDH" : "CCDL";
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validDynamicModes = {"CCDL", "CCDH"};
        modeStr = validDynamicModes.contains(requestedMode) ? requestedMode : QStringLiteral("CCDH");
    }

    qDebug() << "[Chroma6310][setDynamicCurrent] baseMode=" << baseMode
             << "modeStr=" << modeStr;

    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    // 設定動態電流級別 (L1, L2)
    int segs = getNumSegments();
    for (int i = 0; i < segs && i < param.levels.size(); ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;

        double value    = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:DYN:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:" + segName);
    }

    // 設定動態時間參數 (T1, T2)
    if (!param.timings.isEmpty()) {
        if (param.timings.size() >= 1) {
            // QString cmd = QString("CURR:DYN:T1 %1").arg(param.timings[0]);
            QString cmd = QString("CURR:DYN:T1 %1").arg(param.timings[0], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:T1");
        }
        if (param.timings.size() >= 2) {
            // QString cmd = QString("CURR:DYN:T2 %1").arg(param.timings[1]);
            QString cmd = QString("CURR:DYN:T2 %1").arg(param.timings[1], 0, 'f', 10);
            sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:T2");
        }
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310] Only L1/L2 supported for dynamic mode, ignore L3+";
    }
}

QString Chroma6310::model() const
{
    return "6310";
}

QString Chroma6310::vendor() const
{
    return "Chroma";
}
