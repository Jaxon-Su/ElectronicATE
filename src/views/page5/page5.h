#pragma once
#include <QWidget>
#include <QSplitter>
#include "page5leftpanel.h"
#include "page5centerpanel.h"
#include "page5rightpanel.h"
#include "page5taskpayload.h"

class Page5ViewModel;

// ══════════════════════════════════════════════════════
//  Page5 — 主視圖（純 UI 協調者）
//
//  職責：
//  - 組裝三個子面板 / Splitter / 收折邏輯
//  - 連接所有子面板與 ViewModel 信號
//  - collectPayloads()：Run 前從主執行緒預取 UI 設定
//    並交給 ViewModel::startExecution()
//
//  Worker 執行緒由 ViewModel 全權管理，View 不持有。
//
//  ┌──────────────────────────────────────────────────────┐
//  │  Page5 (main)          Page5ViewModel (main)         │
//  │  runRequested(tasks)                                 │
//  │    collectPayloads() ──→ startExecution(payloads)    │
//  │    stopRequested()   ──→ stopExecution()             │
//  │                     ←── taskStatusChanged()         │
//  │                     ←── logMessage()                │
//  │                     ←── runningChanged()            │
//  └──────────────────────────────────────────────────────┘
// ══════════════════════════════════════════════════════
class Page5 : public QWidget
{
    Q_OBJECT

public:
    explicit Page5(Page5ViewModel* viewModel, QWidget* parent = nullptr);

private:
    void setupUi();
    void setupConnections();

    // Run 啟動前，在主執行緒從 CenterPanel 預取各列設定
    QVector<TaskPayload> collectPayloads(const QVector<RunTask>& tasks) const;

    Page5ViewModel*   m_viewModel    = nullptr;
    QSplitter*        m_mainSplitter = nullptr;

    Page5LeftPanel*   m_leftPanel    = nullptr;
    Page5CenterPanel* m_centerPanel  = nullptr;
    Page5RightPanel*  m_rightPanel   = nullptr;
};
