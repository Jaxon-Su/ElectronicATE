#pragma once
#include <QString>
#include <QVector>
#include <optional>

// ──────────────────────────────────────────────────────────────────────────────
//  Chroma 6310A 系列規格
//  ⚠ struct 定義獨立於 chromaload6310spec，兩系列各自維護，互不依賴
// ──────────────────────────────────────────────────────────────────────────────

// ==================== 資料結構 ====================

struct AccuracySpec {
    double percentOfReading = 0.0;
    double percentOfFS      = 0.0;
    double fixedError       = 0.0;
    QString fixedUnit;
    QString remark;
};

struct CurrentRangeSpec {
    double minCurrent  = 0.0;
    double maxCurrent  = 0.0;
    double resolution  = 0.0;
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
    double t1Min             = 0.0;
    double t1Max             = 0.0;
    double t1Resolution      = 0.0;
    double slewMin           = 0.0;
    double slewMax           = 0.0;
    double slewResolution    = 0.0;
    double accuracy          = 0.0;
    double currentMin        = 0.0;
    double currentMax        = 0.0;
    double currentResolution = 0.0;
    double currentAccuracy   = 0.0;
    QString remark;
};

struct PowerRangeSpec {
    QString mode;               // "CCL" 或 "CCH"
    double  power = 0.0;
    CurrentRangeSpec  currentSpec;
    VoltageRangeSpec  voltageSpec;
    ResistanceSpec    resistanceSpec;
    DynamicSpec       dynamicSpec;
    QString remark;
};

struct ChromaLoadSpec {
    QString model;
    QVector<PowerRangeSpec> ranges;
};

// ──────────────────────────────────────────────────────────────────────────────
//  Chroma 6310A 系列型號
//
//  新增型號（相較於 6310 系列）：
//    63107A  —— 雙通道模組（左:30W/5A, 右:CCL 30W/4A + CCH 250W/40A）
//               本驅動以右通道 CCL/CCH 為主；左通道由 CHAN 選擇
//    63110A  —— LED 模擬負載，500V，100W×2，支援 LEDL/LEDH 模式
//               ⚠ 無程式設計功能（PROGRAM 子系統）
//    63113A  —— 300V，300W (CCL 5A / CCH 20A)
//    63115A  —— 600V，300W (CCL 5A / CCH 20A)
//    63123A  —— 120V，350W (CCL 7A / CCH 70A)，高精度 0.04%
//
//  延續型號（規格與 6310 相同，僅型號後綴不同）：
//    63101A, 63102A, 63103A, 63105A, 63106A, 63108A, 63112A
// ──────────────────────────────────────────────────────────────────────────────

// ==================== 延續型號（與 6310 相同規格）====================

ChromaLoadSpec createChroma63101ASpec();   ///< 80V  20W/200W  CCL 4A / CCH 40A
ChromaLoadSpec createChroma63102ASpec();   ///< 80V  20W/100W  CCL 2A / CCH 20A（100W×2 雙通道）
ChromaLoadSpec createChroma63103ASpec();   ///< 80V  30W/300W  CCL 6A / CCH 60A
ChromaLoadSpec createChroma63105ASpec();   ///< 500V 30W/300W  CCL 1A / CCH 10A
ChromaLoadSpec createChroma63106ASpec();   ///< 80V  60W/600W  CCL 12A / CCH 120A
ChromaLoadSpec createChroma63108ASpec();   ///< 500V 60W/600W  CCL 2A / CCH 20A
ChromaLoadSpec createChroma63112ASpec();   ///< 80V  120W/1200W CCL 24A / CCH 240A

// ==================== 新增型號 ====================

ChromaLoadSpec createChroma63107ASpec();
/**
 *  63107A 右通道規格（左通道 30W/5A 固定單檔）
 *  mode:
 *    CCL  30W   0~4A   80V
 *    CCH  250W  0~40A  80V
 */

ChromaLoadSpec createChroma63110ASpec();
/**
 *  63110A LED 模擬負載
 *  ⚠ 支援 LEDL/LEDH 模式；此處以 CC 電流範圍紀錄
 *  mode:
 *    CCL  100W  0~0.6A  500V（低電流範圍，100V 電壓檔）
 *    CCH  100W  0~2A    500V（高電流範圍，500V 電壓檔）
 */

ChromaLoadSpec createChroma63113ASpec();
/**
 *  63113A 300V 300W
 *  mode:
 *    CCL  300W  0~5A   300V
 *    CCH  300W  0~20A  300V
 */

ChromaLoadSpec createChroma63115ASpec();
/**
 *  63115A 600V 300W
 *  mode:
 *    CCL  300W  0~5A   600V
 *    CCH  300W  0~20A  600V
 */

ChromaLoadSpec createChroma63123ASpec();
/**
 *  63123A 120V 350W，高精度（0.04% + 0.04%F.S.）
 *  mode:
 *    CCL  350W  0~7A   120V
 *    CCH  350W  0~70A  120V
 */

// ==================== 查詢輔助函數 ====================

/**
 * @brief 根據電流值找到 6310A 對應型號的最適功率範圍
 * @param subModel  帶 A 後綴的子型號，例如 "63103A"
 * @param currval   目標電流 (A)
 * @return 找到的 PowerRangeSpec；超出最大電流時回傳最後一個範圍（CCH）；
 *         型號不存在時回傳 nullopt
 */
std::optional<PowerRangeSpec> findPowerRange6310A(const QString& subModel, double currval);

/**
 * @brief 為 6310A 選擇最佳靜態負載模式（CCL / CCH）
 *
 * 決策邏輯：
 *   1. 以 current / 0.95 查找範圍（留 5% 餘量）
 *   2. 驗證 current、power、voltage 均在規格內（95% margin）
 *   3. 直接讀取 PowerRangeSpec::mode，不依賴 index 推斷
 *   4. 無合適範圍時 fallback 到 "CCH"
 *
 * @param subModel  帶 A 後綴的子型號，例如 "63103A"
 * @param current   目標電流 (A)，取 param.levels 最大值
 * @param voltage   預期電壓 (V)
 * @return "CCL" 或 "CCH"
 */
QString selectOptimalLoadMode6310A(const QString& subModel,
                                   double current,
                                   double voltage);
