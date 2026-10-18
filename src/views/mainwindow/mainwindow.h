#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

class MainWindowViewModel;
class MainWindowModel;
class MainPageTabs;
class InstrumentSetupPage;
class TestConditionsPage;
class ManualControlPage;
class CommandConsolePage;
class TestSequencePage;
class WaveformOcrPage;
class WfmViewerPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    InstrumentSetupPage *instrumentSetup() const { return m_instrumentSetup; }
    TestConditionsPage *testConditions() const { return m_testConditions; }
    ManualControlPage *manualControl() const { return m_manualControl; }
    CommandConsolePage *commandConsole() const { return m_commandConsole; }
    TestSequencePage *testSequence() const { return m_testSequence; }
    WaveformOcrPage *waveformOcr() const { return m_waveformOcr; }
    WfmViewerPage *wfmViewer() const { return m_wfmViewer; }

  protected:
    void closeEvent(QCloseEvent *event) override;

  private slots:
    void onSaveConfig();
    void onSaveConfigAs();
    void onLoadConfig();
    void onRequestSaveDialog();
    void onRequestLoadDialog();
    void onShowMessage(const QString &title, const QString &message, int type);

  private:
    MainWindowModel *m_model;
    MainWindowViewModel *m_viewModel;

    MainPageTabs *m_pageTabs = nullptr; // 儲存指標供 run 鎖定使用

    // Pages - 初始化為 nullptr
    InstrumentSetupPage *m_instrumentSetup = nullptr;
    TestConditionsPage *m_testConditions = nullptr;
    ManualControlPage *m_manualControl = nullptr;
    CommandConsolePage *m_commandConsole = nullptr;
    TestSequencePage *m_testSequence = nullptr;
    WaveformOcrPage *m_waveformOcr = nullptr;
    WfmViewerPage *m_wfmViewer = nullptr;

    bool m_closeRequested = false;
    bool m_outputExitConfirmed = false;
    enum class WaveformOcrCloseState { Undecided, Prompting, Exporting, Discarded };
    WaveformOcrCloseState m_waveformOcrCloseState = WaveformOcrCloseState::Undecided;
    bool prepareWaveformOcrClose();
    void cancelClose();
    void disableClosingUi();
    void updateControlPageLock();
    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void setupMessageService();
};
