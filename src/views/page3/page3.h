#pragma once
#include <QWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QTableWidget>
#include <page3viewmodel.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTextEdit>
#include <QCheckBox>
#include "triggerwidgetfactory.h"

class QPushButton;

class Page3 : public QWidget
{
    Q_OBJECT
public:
    explicit Page3(Page3ViewModel* viewModel, QWidget *parent = nullptr);
    ~Page3() override = default;

    // UI 同步
    void syncUIToViewModel();
    void resetUIFromViewModel();
    void debugCurrentSelections() const;

public slots:
    void onHeadersChanged(const QStringList &hdr);
    void onRowLabelsChanged(const QStringList &lbl);
    void onTitlesUpdated(LoadKind type, const QStringList& titles);
    void onPage1ConfigChanged(const Page1Config &cfg);
    void onRestoreSelections(LoadKind type, int index, const QString& text);
    void forceButtonOff(LoadKind type);

signals:
    // 輸入控制信號
    void inputToggled(bool on);
    void inputChanged();

    // Load 控制信號
    void loadToggled(bool on);
    void loadChanged();

    // Dynamic Load 控制信號
    void dyloadToggled(bool on);
    void dyloadChanged();

    // 選擇變更（統一信號）
    void selectedChanged(LoadKind type, int index, const QString& text);

    // Trigger 相關
    void triggerWidgetCreated(const QString& modelName, QObject* triggerController);
    void triggerWidgetDestroyed();

private:
    // 初始化
    void initializeUI();
    void buildLayout();
    void applyStyles();
    void setupConnections();

    // 連接輔助（減少重複代碼）
    void connectToggleButton(QPushButton* btn, void (Page3::*signal)(bool));
    void connectComboBox(QComboBox* cmb, LoadKind kind);
    void connectChangeButton(QPushButton* btn, void (Page3::*signal)());

    // UI 輔助
    QPushButton* createPushButton(const QString &text, const QString &objectName = QString()) const;
    void loadLock();  // Load 和 DyLoad 互鎖

    // Trigger 相關
    void setTriggerModel(const QString& modelName);
    void createTriggerWidget();

    // ComboBox 恢復
    void restoreComboBoxSelection(LoadKind type, int index, const QString& text);

private:
    Page3ViewModel *vm = nullptr;

    // Trigger
    QString m_currentTriggerModel;
    QObject* m_triggerController = nullptr;

    // UI 組件 - Input Group
    QGroupBox   *grpInput   = nullptr;
    QComboBox   *cmbInput   = nullptr;
    QPushButton *btnInput   = nullptr;
    QPushButton *btnChange  = nullptr;

    // UI 組件 - Load Group
    QGroupBox   *grpLoad    = nullptr;
    QComboBox   *cmbLoad    = nullptr;
    QPushButton *btnLoadOn  = nullptr;
    QPushButton *btnLoadChg = nullptr;

    // UI 組件 - Dynamic Load Group
    QGroupBox   *grpDyload    = nullptr;
    QComboBox   *cmbDyload    = nullptr;
    QPushButton *btnDyloadOn  = nullptr;
    QPushButton *btnDyloadChg = nullptr;
    QCheckBox   *chkDyload    = nullptr;

    // UI 組件 - Relay Group
    QGroupBox   *grpRelay    = nullptr;
    QComboBox   *cmbRelay    = nullptr;
    QPushButton *btnRelayOn  = nullptr;
    QPushButton *btnRelayChg = nullptr;

    // UI 組件 - Capture Group
    QGroupBox   *grpCap     = nullptr;
    QPushButton *btnPic     = nullptr;
    QPushButton *btnCsv     = nullptr;

    // UI 組件 - Trigger & Table
    QWidget*     grpTrigger = nullptr;
    QTableWidget *tblTop    = nullptr;
    QWidget      *m_ctrlArea = nullptr;
    QHBoxLayout  *m_ctrlLay  = nullptr;

    // 常量
    static constexpr int kBtnWidth  = 75;
    static constexpr int kBtnHeight = 25;
};
