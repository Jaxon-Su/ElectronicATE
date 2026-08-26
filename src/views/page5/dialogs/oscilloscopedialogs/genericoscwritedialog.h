#pragma once
#include "oscwritedialogbase.h"
#include <QCheckBox>

// ══════════════════════════════════════════════════════
//  GenericOscWriteDialog — 未知型號的通用後備面板
//
//  當示波器型號不在已知清單中時顯示此面板。
//  提供基本設定：Timescale + Acquire Mode。
//
//  ★ 新增第二款示波器後，此 Dialog 僅作為最後後備，
//    優先使用型號專屬 Dialog（如 RigolWriteDialog）。
// ══════════════════════════════════════════════════════
class GenericOscWriteDialog : public OscWriteDialogBase
{
    Q_OBJECT

public:
    explicit GenericOscWriteDialog(Oscilloscope*      scope,
                                   const QString&     configuredModel,
                                   const QVariantMap& initCfg,
                                   int                seqNo,
                                   const QString&     extName,
                                   QWidget*           parent = nullptr);

protected:
    QWidget* buildContentWidget() override;
    void     saveConfig()         override;

private:
    QCheckBox* m_basicIgnore = nullptr;
    QCheckBox* m_trigIgnore  = nullptr;
    QComboBox* m_timescale   = nullptr;
    QComboBox* m_acqMode     = nullptr;
    QComboBox* m_trigSource  = nullptr;
    QComboBox* m_trigEdge    = nullptr;
};
