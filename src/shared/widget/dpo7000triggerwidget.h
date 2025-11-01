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
class DPO7000TriggerController;

class DPO7000TriggerWidget : public QGroupBox {
    Q_OBJECT

public:
    explicit DPO7000TriggerWidget(QWidget* parent = nullptr);
    ~DPO7000TriggerWidget();
    QObject* getTriggerController() const { return m_controller; }

    void cleanup();

private slots:
    void setupUI();
    void createControls();
    void setupLayout();

private:
    QComboBox* m_cmbTrigType = nullptr;
    QComboBox* m_cmbTrigSource = nullptr;
    QPushButton* m_btnTrigRising = nullptr;
    QPushButton* m_btnTrigFalling = nullptr;
    QPushButton* m_btnTrigBoth = nullptr;
    QPushButton* m_btnTrigAuto = nullptr;
    QPushButton* m_btnTrigNorm = nullptr;
    QPushButton* m_btnTrigSingle = nullptr;
    QPushButton* m_btnTrigSet = nullptr;
    QPushButton* m_btnRunstop = nullptr;
    QPushButton* m_btntrig_Steady = nullptr;
    SmartStepSpinBox* m_spinTrigLevel = nullptr;
    SmartStepSpinBox* m_autoTrigScale = nullptr;
    SmartStepSpinBox* m_autoTrigTarget = nullptr;
    QLabel* m_lblTrigStatus = nullptr;

    QPointer<QObject> m_controller;
};
