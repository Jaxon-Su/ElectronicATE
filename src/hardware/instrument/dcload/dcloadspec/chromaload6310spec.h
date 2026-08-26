#pragma once
#include <QString>
#include <QVector>
#include <optional>

struct AccuracySpec {
    double percentOfReading = 0.0;
    double percentOfFS = 0.0;
    double fixedError = 0.0;
    QString fixedUnit;
    QString remark;
};

struct CurrentRangeSpec {
    double minCurrent = 0.0;
    double maxCurrent = 0.0;
    double resolution = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct VoltageRangeSpec {
    double minVoltage = 0.0;
    double maxVoltage = 0.0;
    double resolution = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct ResistanceRangeAccuracy {
    double minOhm = 0.0;
    double maxOhm = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct ResistanceSpec {
    QVector<ResistanceRangeAccuracy> rangeAccuracyList;
    int resolutionBits = 0;
    QString remark;
};

struct DynamicSpec {
    double t1Min = 0.0;
    double t1Max = 0.0;
    double t1Resolution = 0.0;
    double slewMin = 0.0;
    double slewMax = 0.0;
    double slewResolution = 0.0;
    double accuracy = 0.0;
    double currentMin = 0.0;
    double currentMax = 0.0;
    double currentResolution = 0.0;
    double currentAccuracy = 0.0;
    QString remark;
};

struct PowerRangeSpec {
    QString mode;               // "CCL" 或 "CCH"（明確標示，不依賴 remark 解析）
    double power = 0.0;
    CurrentRangeSpec currentSpec;
    VoltageRangeSpec voltageSpec;
    ResistanceSpec resistanceSpec;
    DynamicSpec dynamicSpec;
    QString remark;
};

struct ChromaLoadSpec {
    QString model;
    QVector<PowerRangeSpec> ranges;
};

ChromaLoadSpec createChroma63101Spec();
ChromaLoadSpec createChroma63102Spec();
ChromaLoadSpec createChroma63103Spec();
ChromaLoadSpec createChroma63105Spec();
ChromaLoadSpec createChroma63106Spec();
ChromaLoadSpec createChroma63108Spec();
ChromaLoadSpec createChroma63112Spec();

/**
 * @brief 根據電流值找到最適合的功率範圍
 * @param subModel 子型號字串，例如 "63103"
 * @param currval  目標電流 (A)
 * @return 找到的 PowerRangeSpec；若超過最大電流則回傳最後一個範圍（CCH）；
 *         若 subModel 不存在則回傳 nullopt
 */
std::optional<PowerRangeSpec> findPowerRange(const QString& subModel, double currval);

/**
 * @brief 選擇最佳靜態負載模式（CCL / CCH）
 *
 * 決策依據：
 *   1. 找到第一個滿足 current <= maxCurrent * 0.95 的範圍
 *   2. 再驗證 power 與 voltage 是否在規格內（95% margin）
 *   3. 直接讀取 PowerRangeSpec::mode 欄位，不依賴 remark 字串解析
 *   4. 若無合適範圍，fallback 到 CCH
 *
 * @param subModel 子型號字串，例如 "63103"
 * @param current  目標電流 (A)，取所有 levels 的最大值傳入
 * @param voltage  預期電壓 (V)
 * @return "CCL" 或 "CCH"
 */
QString selectOptimalLoadMode(const QString& subModel,
                              double current,
                              double voltage);
