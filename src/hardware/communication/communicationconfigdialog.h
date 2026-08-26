#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QGroupBox>
#include "communicationconfig.h"

class CommunicationConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CommunicationConfigDialog(QWidget* parent = nullptr);
    ~CommunicationConfigDialog() override = default;

    void setConfig(const CommunicationConfig& config);
    CommunicationConfig getConfig() const;

    // 設定可用的協議列表（可選，預設顯示全部）
    void setAvailableProtocols(const QList<ProtocolType>& protocols);

private slots:
    void onProtocolChanged(int index);
    void onTestConnection();
    void onRefreshSerialPorts();

private:
    void setupUI();
    void createGpibPage();
    void createTcpPage();
    void createSerialPage();
    void createModbusRtuPage();
    void createModbusTcpPage();
    void createUsbPage();

    void updatePageFromProtocol(ProtocolType protocol);
    void populateSerialPorts(QComboBox* comboBox);

    // 主要控件
    QComboBox* m_cmbProtocol = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    QPushButton* m_btnTest = nullptr;
    QLabel* m_lblStatus = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;

    // ===== GPIB 頁面控件 =====
    QWidget* m_pageGpib = nullptr;
    QSpinBox* m_spnGpibBoard = nullptr;
    QSpinBox* m_spnGpibAddress = nullptr;
    QSpinBox* m_spnGpibSecondary = nullptr;

    // ===== TCP 頁面控件 =====
    QWidget* m_pageTcp = nullptr;
    QLineEdit* m_edtIpAddress = nullptr;
    QSpinBox* m_spnPort = nullptr;
    QComboBox* m_cmbTcpProtocol = nullptr;

    // ===== Serial 頁面控件 =====
    QWidget* m_pageSerial = nullptr;
    QComboBox* m_cmbPortName = nullptr;
    QComboBox* m_cmbBaudRate = nullptr;
    QComboBox* m_cmbDataBits = nullptr;
    QComboBox* m_cmbParity = nullptr;
    QComboBox* m_cmbStopBits = nullptr;
    QComboBox* m_cmbFlowControl = nullptr;
    QPushButton* m_btnRefreshPorts = nullptr;

    // ===== Modbus RTU 頁面控件 =====
    QWidget* m_pageModbusRtu = nullptr;
    QComboBox* m_cmbModbusRtuPort = nullptr;
    QComboBox* m_cmbModbusRtuBaud = nullptr;
    QComboBox* m_cmbModbusRtuDataBits = nullptr;
    QComboBox* m_cmbModbusRtuParity = nullptr;
    QComboBox* m_cmbModbusRtuStopBits = nullptr;
    QSpinBox* m_spnModbusRtuSlaveId = nullptr;
    QPushButton* m_btnRefreshModbusPorts = nullptr;

    // ===== Modbus TCP 頁面控件 =====
    QWidget* m_pageModbusTcp = nullptr;
    QLineEdit* m_edtModbusTcpIp = nullptr;
    QSpinBox* m_spnModbusTcpPort = nullptr;
    QSpinBox* m_spnModbusTcpSlaveId = nullptr;

    // ===== USB 頁面控件 =====
    QWidget* m_pageUsb = nullptr;
    QLineEdit* m_edtVendorId = nullptr;
    QLineEdit* m_edtProductId = nullptr;
    QLineEdit* m_edtSerialNumber = nullptr;

    // 協議類型與頁面索引映射
    QMap<ProtocolType, int> m_protocolPageMap;

    // 當前選擇的主協議類型
    ProtocolType m_currentMainProtocol = ProtocolType::Unknown;
};
