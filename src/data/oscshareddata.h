#pragma once
#include <QStringList>
#include <QString>

// ══════════════════════════════════════════════════════
//  OscSharedData — 示波器 Dialog 共用靜態資料
//
//  所有示波器 Dialog 類別 include 此檔取得共用常數，
//  避免在各 .cpp 重複宣告導致 ODR 問題。
//
//  ★ 新增示波器型號時：
//  - 共用常數     → 直接使用此處已有常數
//  - 型號專屬常數 → 在各自的 dialog .cpp 內宣告（匿名 namespace）
//  - 型號識別     → 在此新增一個 is___Family() 函式
// ══════════════════════════════════════════════════════
namespace OscSharedData
{

// ── 通用時間軸（1ns ~ 10s，所有型號共用）──────────────
inline const QStringList TIMESCALES = {
    "1ns","2ns","5ns","10ns","20ns","50ns",
    "100ns","200ns","500ns",
    "1us","2us","5us","10us","20us","50us",
    "100us","200us","500us",
    "1ms","2ms","5ms","10ms","20ms","50ms",
    "100ms","200ms","500ms",
    "1s","2s","5s","10s"
};

// ── DPO7000 / DPO5000 系列專屬 ───────────────────────
inline const QStringList DPO7000_SCALES = {
    "1mV","2mV","5mV","10mV","20mV","50mV",
    "100mV","200mV","500mV",
    "1V","2V","5V","10V","20V","50V","100V",
    "Custom..."
};
inline const QStringList DPO7000_BWS = {"Full","500MHz","250MHz","20MHz"};

// ── DPO4000 / MSO4000 系列專屬 ────────────────────────
inline const QStringList DPO4000_SCALES = {
    "1mV","2mV","5mV","10mV","20mV","50mV",
    "100mV","200mV","500mV",
    "1V","2V","5V","10V","20V","50V","100V",
    "Custom..."
};
inline const QStringList DPO4000_BWS = {"Full","250MHz","20MHz"};
inline const QStringList DPO4000_ACQ_MODES = {
    "SAMple","PEAKdetect","HIRes","AVErage","ENVelope"
};

// ── MSO 4/5/6 系列專屬 ────────────────────────────────
// Scale 與 DPO7000 共用 DPO7000_SCALES（SCPI 語法相同）
//
// Coupling：DCR 取代舊款的 GND
//   CH<x>:COUPling {AC|DC|DCR}
inline const QStringList MSO456_COUPLINGS = {"DC","AC","DCR"};
//
// Bandwidth：數值型顯示（MSO44B 最大 200 MHz）
//   CH<x>:BANdwidth {FULl|<NR3>}
//   ViewModel 送出前轉換："Full"→"FULl"，"200MHz"→"200e6"
inline const QStringList MSO456_BWS = {"Full","200MHz","100MHz","20MHz"};
//
// Termination：每通道獨立（DPO 系列無此設定）
//   CH<x>:TERmination {50|1000000}
//   ViewModel 送出前轉換："1MΩ"→"1e6"，"50Ω"→"50"
inline const QStringList MSO456_TERMINATIONS = {"1MΩ","50Ω"};
//
// Horizontal Mode：MSO 新增
//   HORizontal:MODe {AUTO|MANual}
//   AUTO   = 儀器自動決定 Sample Rate / Record Length（兩者 read-only）
//   MANual = 可手動設定 Sample Rate / Record Length（三參數互相牽制）
inline const QStringList MSO456_HORZ_MODES = {"AUTO","MANual"};
//
// MANual 模式下，調整 Sample Rate 時優先變動哪個參數：
//   HORizontal:MODe:MANual:CONFIGure {HORIZontalscale|RECORDLength}
//   HORIZontalscale → Timescale 跟著 SR 變，Record Length 盡量維持
//   RECORDLength    → Record Length 跟著 SR 變，Timescale 盡量維持
inline const QStringList MSO456_HORZ_CONFIGS = {"HORIZontalscale","RECORDLength"};
//
// Sample Rate 常用預設值（MSO44B 最大 6.25 GS/s）
//   HORizontal:MODe:SAMPLERate <NR1>（單位：S/s）
//   ViewModel 送出前轉換顯示字串 → NR1：
//     "6.25GS/s"  → 6.25e9
//     "312.5MS/s" → 312.5e6
//     "1.25MS/s"  → 1.25e6
//   ★ MANual 模式下才可設定；AUTO 模式時 UI 禁用
inline const QStringList MSO456_SAMPLE_RATES = {
    "6.25GS/s","3.125GS/s","1.5625GS/s",
    "625MS/s","312.5MS/s","156.25MS/s",
    "62.5MS/s","31.25MS/s","12.5MS/s",
    "6.25MS/s","3.125MS/s","1.25MS/s"
};
//
// Record Length 常用預設值
//   HORizontal:MODe:RECOrdlength <NR1>（單位：samples）
//   ViewModel 送出前轉換：
//     "1k"→1000，"100k"→100000，"10M"→10000000，"62.5M"→62500000
//   ★ MANual 模式下才可設定；AUTO 模式時 UI 禁用
//   ★ 實際可選值依型號與 Sample Rate 組合而異，儀器會自動限制
inline const QStringList MSO456_RECORD_LENGTHS = {
    "1k","2k","5k","10k","20k","50k",
    "100k","200k","500k",
    "1M","2M","5M","10M","20M","50M","62.5M"
};

// ── 多型號共用 ────────────────────────────────────────
inline const QStringList COUPLINGS = {"DC","AC","GND"};   // DPO 系列用
inline const QStringList ACQ_MODES = {
    "SAMple","PEAKdetect","HIRes","AVErage","ENVelope","WFMDB"
};

// ── Edge Trigger 觸發設定（多型號共用）────────────────
// TRIGger:A:EDGE:SOUrce
// DPO7000 / DPO4000: EXT / LINE 可用
inline const QStringList TRIG_SOURCES_DPO = {
    "CH1","CH2","CH3","CH4","EXT","LINE"
};
// MSO 4/5/6: AUXiliary 取代 EXT
inline const QStringList TRIG_SOURCES_MSO = {
    "CH1","CH2","CH3","CH4","LINE","AUX"
};
// TRIGger:A:EDGE:SLOpe {RISe|FALL|EITher}
// Driver 負責將顯示名稱轉為 SCPI 關鍵字（Rising→RISe 等）
inline const QStringList TRIG_EDGES = {"Rising","Falling","Either"};

// ══════════════════════════════════════════════════════
//  型號家族判斷（工廠用）
//
//  ★ 新增示波器型號：在此新增對應的 is___Family() 函式
//  ★ 其他地方不需修改（開放/封閉原則）
//
//  判斷優先順序（createWriteOscilloscopeDialog 依此順序呼叫）：
//    1. isMSO456Family  → MSO44BWriteDialog
//    2. isDPO7000Family → DPO7000WriteDialog
//    3. isDPO4000Family → DPO4000WriteDialog
//    4. （未來新增）
//    5. 後備            → GenericOscWriteDialog
//
//  ⚠ 順序注意：isMSO456Family 必須在 isDPO7000Family 之前判斷，
//    因為舊版曾以 "MSO" 字串作為 DPO7000 家族的判斷條件。
// ══════════════════════════════════════════════════════

// MSO 4/5/6 系列：MSO44/46/54/56/58/64…（含 B/LP 變體）
// 明確排除舊款 MSO4000 系列（由 isDPO4000Family 處理）
inline bool isMSO456Family(const QString& model)
{
    return model.contains("MSO", Qt::CaseInsensitive)
    && !model.contains("MSO4000", Qt::CaseInsensitive);
}

// DPO7000 / DPO5000 系列（不含 MSO，MSO 已由 isMSO456Family 處理）
inline bool isDPO7000Family(const QString& model)
{
    return model.contains("DPO7000", Qt::CaseInsensitive)
    || model.contains("DPO5000", Qt::CaseInsensitive);
}

// DPO4000 / MSO4000 系列（舊款 4 通道，頻寬上限 250 MHz）
inline bool isDPO4000Family(const QString& model)
{
    return model.contains("DPO4000", Qt::CaseInsensitive)
    || model.contains("MSO4000", Qt::CaseInsensitive);
}

// 未來範例（取消註解並實作對應 Dialog 即可）：
// inline bool isRigolFamily(const QString& model) {
//     return model.contains("DS1",  Qt::CaseInsensitive)
//         || model.contains("DHO",  Qt::CaseInsensitive);
// }

} // namespace OscSharedData
