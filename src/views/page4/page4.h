#pragma once

#include <QWidget>
#include "page4config.h"

class Page4ViewModel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QPlainTextEdit;
class QLabel;
class QSpinBox;

class Page4 : public QWidget
{
    Q_OBJECT

public:
    explicit Page4(Page4ViewModel *viewModel, QWidget *parent = nullptr);
    ~Page4() override = default;

private slots:
    // 連線按鈕
    void onConnectClicked();
    void onDisconnectClicked();

    // 指令操作
    void onSendClicked();
    void onClearClicked();

    // 常用指令按鈕
    void onIDNClicked();
    void onRSTClicked();
    void onCLSClicked();
    void onOPCClicked();

    // 從歷史選單填入指令
    void onCommandHistorySelected(const QString &command);

    // ViewModel 事件
    void onConnectionStatusChanged(ConnectionStatus status, const QString &message);
    void onResponseReceived(const QString &command, const QString &response);
    void onErrorOccurred(const QString &error);
    void onHistoryUpdated();

private:
    void setupUI();
    void setupConnections();
    void updateConnectionUI(bool connected);
    void appendLog(const QString &text, const QString &color = "white");
    void updateAddressHistory();
    void updateCommandHistory();

    Page4ViewModel *m_viewModel = nullptr;

    // UI 元件 - 連線區
    QComboBox   *m_addressCombo   = nullptr;
    QPushButton *m_connectBtn     = nullptr;
    QPushButton *m_disconnectBtn  = nullptr;
    QLabel      *m_statusLabel    = nullptr;
    QSpinBox    *m_timeoutSpin    = nullptr;

    // UI 元件 - 指令區
    // m_commandEdit  : 多行輸入，支援自動換行（取代原本的可編輯 QComboBox）
    // m_commandHistory: 唯讀歷史下拉選單，選取後填入 m_commandEdit
    QPlainTextEdit *m_commandEdit    = nullptr;
    QComboBox      *m_commandHistory = nullptr;
    QPushButton    *m_sendBtn        = nullptr;
    QPushButton    *m_clearBtn       = nullptr;

    // UI 元件 - 常用指令
    QPushButton *m_idnBtn         = nullptr;
    QPushButton *m_rstBtn         = nullptr;
    QPushButton *m_clsBtn         = nullptr;
    QPushButton *m_opcBtn         = nullptr;

    // UI 元件 - 日誌
    QTextEdit   *m_logView        = nullptr;
};
