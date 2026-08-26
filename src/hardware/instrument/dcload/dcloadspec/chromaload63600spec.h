#pragma once
#include <QString>
#include <QVector>
#include <optional>

// ==================== 規格結構定義 ====================

struct AccuracySpec63600 {
    double percentOfReading = 0.0;
    double percentOfFS = 0.0;
    double fixedError = 0.0;
    QString fixedUnit;
    QString remark;
};

struct CurrentRangeSpec63600 {
    double minCurrent = 0.0;
    double maxCurrent = 0.0;
    double resolution = 0.0;
    AccuracySpec63600 accuracy;
    QString remark;
};

struct VoltageRangeSpec63600 {
    double minVoltage = 0.0;
    double maxVoltage = 0.0;
    double resolution = 0.0;
    AccuracySpec63600 accuracy;
    QString remark;
};

struct ResistanceRangeAccuracy63600 {
    double minOhm = 0.0;
    double maxOhm = 0.0;
    AccuracySpec63600 accuracy;
    QString remark;
};

struct ResistanceSpec63600 {
    QVector<ResistanceRangeAccuracy63600> rangeAccuracyList;
    int resolutionBits = 0;
    QString remark;
};

struct DynamicSpec63600 {
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

struct PowerRangeSpec63600 {
    QString mode;               // "CCL", "CCM", "CCH"（明確標示，不依賴 index 或 remark 解析）
    double power = 0.0;
    CurrentRangeSpec63600 currentSpec;
    VoltageRangeSpec63600 voltageSpec;
    ResistanceSpec63600 resistanceSpec;
    DynamicSpec63600 dynamicSpec;
    QString remark;
};

struct ChromaLoad63600Spec {
    QString model;
    QVector<PowerRangeSpec63600> ranges;  // 三個範圍: CCL, CCM, CCH
};

// ==================== 規格創建函數 ====================

ChromaLoad63600Spec createChroma636108020Spec();
ChromaLoad63600Spec createChroma636308060Spec();
ChromaLoad63600Spec createChroma636408080Spec();
ChromaLoad63600Spec createChroma6364015060Spec();
ChromaLoad63600Spec createChroma6363060015Spec();

// ==================== 輔助函數 ====================

/**
 * @brief 根據電流值找到最適合的功率範圍
 * @param subModel 子型號字串，例如 "63640-80-80"
 * @param currval  目標電流 (A)
 * @return 找到的 PowerRangeSpec63600；若超過最大電流則回傳最後一個範圍（CCH）；
 *         若 subModel 不存在則回傳 nullopt
 */
std::optional<PowerRangeSpec63600> findPowerRange63600(const QString& subModel, double currval);

/**
 * @brief 選擇最佳靜態負載模式（CCL / CCM / CCH）
 *
 * 決策依據：
 *   1. 找到第一個滿足 current <= maxCurrent * 0.95 的範圍
 *   2. 再驗證 power 與 voltage 是否在規格內（95% margin）
 *   3. 直接讀取 PowerRangeSpec63600::mode 欄位，不依賴 index 推斷
 *   4. 若無合適範圍，fallback 到 CCH
 *
 * @param subModel 子型號字串，例如 "63640-80-80"
 * @param current  目標電流 (A)，取所有 levels 的最大值傳入
 * @param voltage  預期電壓 (V)
 * @return "CCL", "CCM", 或 "CCH"
 */
QString selectOptimalLoadMode63600(const QString& subModel,
                                   double current,
                                   double voltage);
