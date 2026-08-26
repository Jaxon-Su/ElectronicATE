#pragma once
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPointer>

class SmartStepSpinBox;
class MSOSeries456TriggerController;

// ─────────────────────────────────────────────────────────────────────────────
//  MSOSeries456TriggerWidget
//
//  支援機型: MSO44(B) / MSO46(B) / MSO54(B) / MSO56(B) / MSO58(B) /
//            MSO58LP / MSO64(B) / MSO66B / MSO68B / LPD64
//
//  與 DPO7000TriggerWidget 的差異：
//    1. 建構子額外接受 totalChannels 參數（預設 4），
//       以動態填充 Source ComboBox（CH1~CH4 / CH6 / CH8）
//    2. 新增 setChannelCount() 供外部在建立後動態調整
//    3. 其餘 UI 結構、objectName 與 DPO7000TriggerWidget 完全相同，
//       確保 MSOSeries456TriggerController 的 findChild 可正確運作
// ─────────────────────────────────────────────────────────────────────────────
class MSOSeries456TriggerWidget : public QGroupBox {
    Q_OBJECT

public:
    explicit MSOSeries456TriggerWidget(int totalChannels = 4,
                                       QWidget* parent = nullptr);
    ~MSOSeries456TriggerWidget();

    QObject* getTriggerController() const { return m_controller; }

    // 動態更新 Source ComboBox 的通道數（4 / 6 / 8）
    // 應在 widget 顯示前呼叫；若已顯示則會清空並重新填入
    void setChannelCount(int totalChannels);

    void cleanup();

private:
    void setupUI();
    void createControls();
    void setupLayout();
    void populateSourceComboBox();

    int m_totalChannels = 4;   // 由建構子或 setChannelCount() 設定

    // ── UI 元件（objectName 與 DPO7000TriggerWidget 相同）─────────────────
    QComboBox*        m_cmbTrigType    = nullptr;
    QComboBox*        m_cmbTrigSource  = nullptr;
    QPushButton*      m_btnTrigRising  = nullptr;
    QPushButton*      m_btnTrigFalling = nullptr;
    QPushButton*      m_btnTrigBoth    = nullptr;
    QPushButton*      m_btnTrigAuto    = nullptr;
    QPushButton*      m_btnTrigNorm    = nullptr;
    QPushButton*      m_btnTrigSingle  = nullptr;
    QPushButton*      m_btnTrigSet     = nullptr;
    QPushButton*      m_btnRunstop     = nullptr;
    QPushButton*      m_btnGetValue    = nullptr;
    QPushButton*      m_btntrig_Steady = nullptr;
    SmartStepSpinBox* m_spinTrigLevel  = nullptr;
    SmartStepSpinBox* m_spinLevelStep  = nullptr;
    SmartStepSpinBox* m_autoTrigScale  = nullptr;
    SmartStepSpinBox* m_autoTrigTarget = nullptr;
    QLabel*           m_lblTrigStatus  = nullptr;
    QLabel*           m_lblMeasMax     = nullptr;
    QLabel*           m_lblMeasMin     = nullptr;
    QLabel*           m_lblMeasRms     = nullptr;
    QLabel*           m_lblMeasMean    = nullptr;
    QLabel*           m_lblMeasAbsPeak = nullptr;

    QPointer<QObject> m_controller;
};
