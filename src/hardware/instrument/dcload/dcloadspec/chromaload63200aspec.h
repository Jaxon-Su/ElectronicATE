#pragma once
#include <QString>
#include <QVector>
#include <optional>

// ==================== 規格結構定義 ====================

struct AccuracySpec63200A {
    double percentOfReading = 0.0;
    double percentOfFS      = 0.0;
    double fixedError       = 0.0;
    QString fixedUnit;
    QString remark;
};

struct CurrentRangeSpec63200A {
    double minCurrent  = 0.0;
    double maxCurrent  = 0.0;
    double resolution  = 0.0;   // A
    AccuracySpec63200A accuracy;
    QString remark;
};

struct VoltageRangeSpec63200A {
    double minVoltage  = 0.0;
    double maxVoltage  = 0.0;
    double resolution  = 0.0;   // V
    AccuracySpec63200A accuracy;
    QString remark;
};

/**
 * 動態規格
 *   T1/T2 範圍：10μs ~ 99999.999ms，解析度 1μs（手冊 4-22 頁）
 *   Slew Rate 依型號與檔位不同（見規格表）
 */
struct DynamicSpec63200A {
    // T1 / T2
    double t1Min        = 0.00001;   // s (10μs)
    double t1Max        = 99.999999; // s (99999.999ms)
    double t1Resolution = 0.000001;  // s (1μs)
    // Slew Rate (A/μs)
    double slewMin      = 0.0;
    double slewMax      = 0.0;
    double slewResolution = 0.0;
    double accuracy     = 0.0;       // 5% ±10μs (固定)
    // Current
    double currentMin   = 0.0;
    double currentMax   = 0.0;
    double currentResolution = 0.0;
    double currentAccuracy   = 0.0;  // 0.2% F.S
    QString remark;
};

/**
 * 單一電流檔位規格（CCL / CCM / CCH）
 * 注意：63200A 電壓量測檔位與電流檔位對應，例如 150V 型號：
 *   CCL(Low)    → Vrange = 16V
 *   CCM(Middle) → Vrange = 80V
 *   CCH(High)   → Vrange = 150V
 */
struct PowerRangeSpec63200A {
    QString mode;                        // "CCL", "CCM", "CCH"
    double  power = 0.0;                 // W（該檔位可用功率）
    CurrentRangeSpec63200A currentSpec;
    VoltageRangeSpec63200A voltageSpec;
    DynamicSpec63200A dynamicSpec;
    QString remark;
};

struct ChromaLoad63200ASpec {
    QString model;
    QVector<PowerRangeSpec63200A> ranges; // 3 ranges: CCL, CCM, CCH
};

// ==================== 規格創建函數（150V 系列） ====================
ChromaLoad63200ASpec createChroma63202A_150_200Spec();
ChromaLoad63200ASpec createChroma63203A_150_300Spec();
ChromaLoad63200ASpec createChroma63204A_150_400Spec();
ChromaLoad63200ASpec createChroma63205A_150_500Spec();
ChromaLoad63200ASpec createChroma63206A_150_600Spec();
ChromaLoad63200ASpec createChroma63208A_150_800Spec();

// ==================== 規格創建函數（600V 系列） ====================
ChromaLoad63200ASpec createChroma63202A_600_140Spec();
ChromaLoad63200ASpec createChroma63205A_600_350Spec();
ChromaLoad63200ASpec createChroma63206A_600_420Spec();
ChromaLoad63200ASpec createChroma63224A_600_1680Spec();

// ==================== 規格創建函數（1200V 系列） ====================
ChromaLoad63200ASpec createChroma63202A_1200_80Spec();
ChromaLoad63200ASpec createChroma63204A_1200_160Spec();
ChromaLoad63200ASpec createChroma63205A_1200_200Spec();
ChromaLoad63200ASpec createChroma63206A_1200_240Spec();
ChromaLoad63200ASpec createChroma63208A_1200_320Spec();

// ==================== 輔助函數 ====================

/**
 * @brief 根據電流值找到最適合的功率範圍（CCL/CCM/CCH）
 * @param subModel 子型號，如 "63205A-150-500"
 * @param currval  目標電流 (A)
 * @return 找到的 PowerRangeSpec63200A；超出最大電流回傳 CCH；未知型號回傳 nullopt
 */
std::optional<PowerRangeSpec63200A> findPowerRange63200A(const QString& subModel,
                                                         double currval);

/**
 * @brief 選擇最佳靜態負載模式（CCL / CCM / CCH）
 *
 * 決策邏輯：
 *   1. 找到第一個滿足 current <= maxCurrent × 0.95 的範圍
 *   2. 驗證功率與電壓是否在規格內（95% margin）
 *   3. 直接讀取 PowerRangeSpec63200A::mode 欄位
 *   4. 若無合適範圍，fallback 到 CCH
 *
 * @param subModel 子型號
 * @param current  目標電流最大值 (A)
 * @param voltage  預期電壓 (V)
 * @return "CCL", "CCM", 或 "CCH"
 */
QString selectOptimalLoadMode63200A(const QString& subModel,
                                    double current,
                                    double voltage);

/**
 * @brief 選擇最佳 CV 負載模式（CVL / CVM / CVH）
 *
 * 依電壓、Ilimit 與功率限制選擇可滿足條件的最小檔位；若超出規格，
 * fallback 到 CVH 並讓硬體保護處理。
 */
QString selectOptimalCVMode63200A(const QString& subModel,
                                  double voltage,
                                  double currentLimit);
