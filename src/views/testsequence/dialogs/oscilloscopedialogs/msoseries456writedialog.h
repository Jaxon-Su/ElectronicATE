#pragma once
#include "oscwritedialogbase.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class HorizontalQuantityEdit;

//  MSOSeries456WriteDialog — Tektronix MSO 4/5/6 系列
//  （MSO44B / MSO46B / MSO54B / MSO56B / MSO58B …）
//
// Horizontal previews assume 10 divisions; the Driver reads actual values at
// execution time. This dialog only edits settings and never accesses hardware.
class MSOSeries456WriteDialog : public OscWriteDialogBase {
    Q_OBJECT

  public:
    explicit MSOSeries456WriteDialog(Oscilloscope* scope, const QString& configuredModel,
                               const QVariantMap& initCfg, int seqNo, const QString& extName,
                               QWidget* parent = nullptr);

  protected:
    QWidget* buildContentWidget() override;
    void saveConfig() override;

  private:
    // Horizontal
    QComboBox* m_horzMode = nullptr;     // HORizontal:MODe
    QComboBox* m_horzInputs = nullptr;
    QComboBox* m_horzLimits = nullptr;
    HorizontalQuantityEdit* m_timescale = nullptr;    // HORizontal:SCAle
    QDoubleSpinBox* m_horzPos = nullptr; // HORizontal:POSition（%）
    HorizontalQuantityEdit* m_sampleRate = nullptr; // HORizontal:MODe:SAMPLERate
    HorizontalQuantityEdit* m_recordLen = nullptr;  // HORizontal:MODe:RECOrdlength
    QLineEdit *m_timeEstimate = nullptr, *m_rateEstimate = nullptr, *m_recordEstimate = nullptr;
    QLabel* m_horizontalHint = nullptr;
    enum HorizontalMode { Auto, Manual };
    enum HorizontalInputs { TimeAndRate, RateAndLength };
    HorizontalInputs m_previousInputs = TimeAndRate;
    double m_derivedValue = 0;

    // Acquire
    QComboBox* m_acqMode = nullptr; // ACQuire:MODe
    QCheckBox* m_fastAcq = nullptr; // ACQuire:FASTAcq:STATE

    // Trigger
    QComboBox* m_trigSource = nullptr; // TRIGger:A:EDGE:SOUrce
    QComboBox* m_trigEdge = nullptr;   // TRIGger:A:EDGE:SLOpe

    // Channel count follows the same configured-model catalog as the Driver factory.
    int m_channelCount = 4;
    struct ChannelWidgets {
        QCheckBox* enable = nullptr;
        QComboBox* termination = nullptr; // CH<x>:TERmination
        QComboBox* scale = nullptr;       // CH<x>:SCAle
        QLineEdit* customEdit = nullptr;  // "Custom..." 時顯示
        QComboBox* coupling = nullptr;    // CH<x>:COUPling（含 DCR）
        QComboBox* bw = nullptr;          // CH<x>:BANdwidth
        QDoubleSpinBox* pos = nullptr;    // CH<x>:POSition（div）
    } m_ch[8];
    QLabel* m_settingsHint = nullptr;
    bool m_horizontalValid = true;

    // Ignore checkboxes（每個 section 頂端，勾選 = 忽略此區段）
    QCheckBox* m_horzIgnore = nullptr;
    QCheckBox* m_acqIgnore = nullptr;
    QCheckBox* m_chIgnore = nullptr;
    QCheckBox* m_trigIgnore = nullptr;

    // 私有輔助
    void updateHorizontalControls();
    void updateApplyState();
    void changeHorizontalInputs();
};
