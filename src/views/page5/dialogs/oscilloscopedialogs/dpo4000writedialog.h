#pragma once
#include "oscwritedialogbase.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

// ══════════════════════════════════════════════════════
//  DPO4000WriteDialog — Tektronix DPO4000 / MSO4000 系列
//
//  與 DPO7000WriteDialog 的主要差異：
//    - Acquire Mode：移除 WFMDB（4000 系列不支援）
//    - Bandwidth：Full / 250MHz / 20MHz（無 500MHz）
//    - 無 WFMDB histogram 相關選項
//
//  ★ 新增 DPO4000 系列子型號不需修改此檔，
//    只需在 OscSharedData::isDPO4000Family() 補上識別條件。
// ══════════════════════════════════════════════════════
class DPO4000WriteDialog : public OscWriteDialogBase
{
    Q_OBJECT

public:
    explicit DPO4000WriteDialog(Oscilloscope*      scope,
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

    QComboBox*      m_timescale  = nullptr;
    QDoubleSpinBox* m_horzPos    = nullptr;
    QComboBox*      m_acqMode    = nullptr;
    QComboBox*      m_trigSource = nullptr;
    QComboBox*      m_trigEdge   = nullptr;

    struct ChannelWidgets {
        QCheckBox*      enable     = nullptr;
        QComboBox*      scale      = nullptr;
        QLineEdit*      customEdit = nullptr;
        QComboBox*      coupling   = nullptr;
        QComboBox*      bw         = nullptr;
        QDoubleSpinBox* pos        = nullptr;
    } m_ch[4];
};
