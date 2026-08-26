#pragma once
#include "oscwritedialogbase.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

// ══════════════════════════════════════════════════════
//  MSO44BWriteDialog — Tektronix MSO 4/5/6 系列
//  （MSO44B / MSO46B / MSO54B / MSO56B / MSO58B …）
//
//  與 DPO7000WriteDialog 相比，主要差異：
//
//  [Horizontal]
//    + HORizontal:MODe {AUTO|MANual}          ← 新增
//    + HORizontal:MODe:MANual:CONFIGure        ← 新增（MANual 才啟用）
//    + HORizontal:MODe:SAMPLERate              ← 新增（MANual 才啟用）
//    + HORizontal:MODe:RECOrdlength            ← 新增（MANual 才啟用）
//
//      三參數關聯式：
//        RecordLength = SampleRate × Timescale(s/div) × 10
//      固定任兩個，第三個由 CONFIGure 決定誰被儀器自動調整。
//
//  [Acquire]
//    + ACQuire:FASTAcq:STATE {ON|OFF}          ← 新增
//      ACQuire:MODe：選項相同（SAMple|PEAKdetect|HIRes|AVErage|ENVelope）
//
//  [Channels]
//    + CH<x>:TERmination {50|1e6}              ← 新增（50Ω / 1MΩ）
//    ~ CH<x>:COUPling {AC|DC|DCR}              ← GND 改為 DCR
//    ~ CH<x>:BANdwidth {FULl|<NR3>}            ← 由固定字串改為數值清單
//      CH<x>:SCAle / POSition：SCPI 語法相同
//
//  ★ 新增 MSO 子型號不需修改此檔，
//    只需在 OscSharedData::isMSO456Family() 補上識別條件。
// ══════════════════════════════════════════════════════
class MSO44BWriteDialog : public OscWriteDialogBase
{
    Q_OBJECT

public:
    explicit MSO44BWriteDialog(Oscilloscope*      scope,
                               const QString&     configuredModel,
                               const QVariantMap& initCfg,
                               int                seqNo,
                               const QString&     extName,
                               QWidget*           parent = nullptr);

protected:
    QWidget* buildContentWidget() override;
    void     saveConfig()         override;

private:
    // ── Horizontal ────────────────────────────────────
    QComboBox*      m_horzMode    = nullptr;  // HORizontal:MODe
    QComboBox*      m_timescale   = nullptr;  // HORizontal:SCAle
    QDoubleSpinBox* m_horzPos     = nullptr;  // HORizontal:POSition（%）
    // ↓ MANual 模式專屬（AUTO 時 UI 禁用）
    QComboBox*      m_horzConfig  = nullptr;  // HORizontal:MODe:MANual:CONFIGure
    QComboBox*      m_sampleRate  = nullptr;  // HORizontal:MODe:SAMPLERate
    QComboBox*      m_recordLen   = nullptr;  // HORizontal:MODe:RECOrdlength

    // ── Acquire ───────────────────────────────────────
    QComboBox*      m_acqMode     = nullptr;  // ACQuire:MODe
    QCheckBox*      m_fastAcq     = nullptr;  // ACQuire:FASTAcq:STATE

    // ── Trigger ───────────────────────────────────────
    QComboBox*      m_trigSource  = nullptr;  // TRIGger:A:EDGE:SOUrce
    QComboBox*      m_trigEdge    = nullptr;  // TRIGger:A:EDGE:SLOpe

    // ── Channels（MSO44B = 4 ch）────────────────────
    struct ChannelWidgets {
        QCheckBox*      enable      = nullptr;
        QComboBox*      termination = nullptr;  // CH<x>:TERmination
        QComboBox*      scale       = nullptr;  // CH<x>:SCAle
        QLineEdit*      customEdit  = nullptr;  // "Custom..." 時顯示
        QComboBox*      coupling    = nullptr;  // CH<x>:COUPling（含 DCR）
        QComboBox*      bw          = nullptr;  // CH<x>:BANdwidth
        QDoubleSpinBox* pos         = nullptr;  // CH<x>:POSition（div）
    } m_ch[4];

    // ── Ignore checkboxes（每個 section 頂端，勾選 = 忽略此區段）──
    QCheckBox*      m_horzIgnore = nullptr;
    QCheckBox*      m_acqIgnore  = nullptr;
    QCheckBox*      m_chIgnore   = nullptr;
    QCheckBox*      m_trigIgnore = nullptr;

    // ── 私有輔助 ──────────────────────────────────────
    void updateManualControls(bool isManual);  // 切換 MANual 專屬控件啟用狀態
};
