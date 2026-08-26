#include "chroma6310a.h"
#include <QDebug>
#include <QSet>
#include <algorithm>
#include "chromaload6310aspec.h"   // selectOptimalLoadMode6310A(), findPowerRange6310A()

// ─────────────────────────────────────────────────────────────────────────
//  setLoadMode
//
//  6310A 在 6310 的基礎上新增：
//    CPL  / CPH   - 定功率低檔 / 高檔
//    LEDL / LEDH  - LED 模擬低壓 / 高壓
// ─────────────────────────────────────────────────────────────────────────
void Chroma6310A::setLoadMode(const QString& mode)
{
    static const QSet<QString> validModes = {
        // ── 繼承自 6310 ──
        "CCL", "CCH", "CCDL", "CCDH", "CRL", "CRH", "CV",
        // ── 6310A 新增 ──
        "CPL", "CPH",     // Constant Power 低檔 / 高檔
        "LEDL", "LEDH"    // LED 模擬低壓 / 高壓
    };

    if (!validModes.contains(mode)) {
        qWarning() << "[Chroma6310A] setLoadMode: Invalid mode:" << mode;
        return;
    }

    write(QString("MODE %1").arg(mode));
}

// ─────────────────────────────────────────────────────────────────────────
//  setStaticCurrent
//
//  邏輯與 Chroma6310::setStaticCurrent 相同，
//  僅將 selectOptimalLoadMode() → selectOptimalLoadMode6310A()
// ─────────────────────────────────────────────────────────────────────────
void Chroma6310A::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    QString modeStr = selectOptimalLoadMode6310A(m_model, maxCurrent, voltage);
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validStaticModes = {"CCL", "CCH"};
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

        double  value   = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:STAT:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310A] setStaticCurrent:" + segName);
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310A] Only L1/L2 supported, ignore L3+";
    }
}

// ─────────────────────────────────────────────────────────────────────────
//  setDynamicCurrent
//
//  邏輯與 Chroma6310::setDynamicCurrent 相同，
//  僅將 selectOptimalLoadMode() → selectOptimalLoadMode6310A()
// ─────────────────────────────────────────────────────────────────────────
void Chroma6310A::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage    = param.expectedVoltage;

    // 先查靜態 mode，再轉換為動態 mode
    QString baseMode = selectOptimalLoadMode6310A(m_model, maxCurrent, voltage);
    QString modeStr  = (baseMode == "CCH") ? "CCDH" : "CCDL";
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode == "NO SETTING") {
        modeStr.clear();
    } else if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE") {
        static const QSet<QString> validDynamicModes = {"CCDL", "CCDH"};
        modeStr = validDynamicModes.contains(requestedMode) ? requestedMode : QStringLiteral("CCDH");
    }

    qDebug() << "[Chroma6310A][setDynamicCurrent] baseMode=" << baseMode
             << "modeStr=" << modeStr;

    if (!modeStr.isEmpty())
        setLoadMode(modeStr);

    // 設定動態電流 L1/L2
    int segs = getNumSegments();
    for (int i = 0; i < segs && i < param.levels.size(); ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;

        double  value   = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd     = QString("CURR:DYN:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310A] setDynamicCurrent:" + segName);
    }

    // 設定 T1/T2 時間參數
    if (param.timings.size() >= 1) {
        sendCommandWithLog(
            QString("CURR:DYN:T1 %1").arg(param.timings[0]),
            "[Chroma6310A] setDynamicCurrent:T1");
    }
    if (param.timings.size() >= 2) {
        sendCommandWithLog(
            QString("CURR:DYN:T2 %1").arg(param.timings[1]),
            "[Chroma6310A] setDynamicCurrent:T2");
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310A] Only L1/L2 supported for dynamic mode, ignore L3+";
    }
}
