#pragma once
#include "oscwritedialogbase.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

// ══════════════════════════════════════════════════════
//  DPO7000WriteDialog — Tektronix DPO7000/DPO5000/MSO 系列
//
//  提供完整設定 UI：
//    - Horizontal：Timescale / Position
//    - Acquire：Mode（6 種）
//    - Channels：CH1~CH4，每通道 Enable / Scale / Coupling / BW / Vert.Pos
//
//  ★ 新增 DPO7000 系列子型號不需修改此檔，
//    只需在 OscSharedData::isDPO7000Family() 補上識別條件。
// ══════════════════════════════════════════════════════
class DPO7000WriteDialog : public OscWriteDialogBase
{
    Q_OBJECT

public:
    explicit DPO7000WriteDialog(Oscilloscope*      scope,
                                const QString&     configuredModel,
                                const QVariantMap& initCfg,
                                int                seqNo,
                                const QString&     extName,
                                QWidget*           parent = nullptr);

protected:
    QWidget* buildContentWidget() override;
    void     saveConfig()         override;

private:
    // ── Ignore checkboxes（每個 section 頂端，勾選 = 忽略此區段）──
    QCheckBox*      m_horzIgnore = nullptr;
    QCheckBox*      m_acqIgnore  = nullptr;
    QCheckBox*      m_chIgnore   = nullptr;
    QCheckBox*      m_trigIgnore = nullptr;

    // ── Widget 集合（buildContentWidget 初始化，saveConfig 讀取）──
    QComboBox*      m_timescale  = nullptr;
    QDoubleSpinBox* m_horzPos    = nullptr;
    QComboBox*      m_acqMode    = nullptr;
    QComboBox*      m_trigSource = nullptr;
    QComboBox*      m_trigEdge   = nullptr;

    struct ChannelWidgets {
        QCheckBox*      enable     = nullptr;
        QComboBox*      scale      = nullptr;
        QLineEdit*      customEdit = nullptr;   // "Custom..." 時顯示
        QComboBox*      coupling   = nullptr;
        QComboBox*      bw         = nullptr;
        QDoubleSpinBox* pos        = nullptr;
    } m_ch[4];
};
