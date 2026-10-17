#pragma once
#include <QWidget>
#include <QSplitter>
#include "testsequenceleftpanel.h"
#include "testsequencecenterpanel.h"
#include "testsequencerightpanel.h"
#include "testsequencetaskpayload.h"

class TestSequenceViewModel;

//  TestSequence — 主視圖（純 UI 協調者）
//
//  職責：
//  - 組裝三個子面板 / Splitter / 收折邏輯
//  - 連接所有子面板與 ViewModel 信號
//    並交給 ViewModel::startExecution()
//
//  Worker 執行緒由 ViewModel 全權管理，View 不持有。
//
// ┌┐
//  │  TestSequence (main)          TestSequenceViewModel (main)         │
//  │  runRequested(tasks)                                 │
// │    stopRequested()   → stopExecution()             │
// │                     ← taskStatusChanged()         │
// │                     ← logMessage()                │
// │                     ← runningChanged()            │
// └┘
class TestSequencePage : public QWidget
{
    Q_OBJECT

  public:
    explicit TestSequencePage(TestSequenceViewModel *viewModel, QWidget *parent = nullptr);

  private:
    void setupUi();
    void setupConnections();

    // Run 啟動前，在主執行緒從 CenterPanel 預取各列設定

    TestSequenceViewModel *m_viewModel = nullptr;
    QSplitter *m_mainSplitter = nullptr;

    TestSequenceLeftPanel *m_leftPanel = nullptr;
    TestSequenceCenterPanel *m_centerPanel = nullptr;
    TestSequenceRightPanel *m_rightPanel = nullptr;
};
