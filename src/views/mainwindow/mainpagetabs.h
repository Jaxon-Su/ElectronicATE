#pragma once

#include <QWidget>
#include <QTabWidget>

class InstrumentSetupPage;
class TestConditionsPage;
class ManualControlPage;
class CommandConsolePage;
class TestSequencePage;
class WaveformOcrPage;
class WfmViewerPage;

class MainPageTabs : public QWidget {
    Q_OBJECT

  public:
    explicit MainPageTabs(QWidget* parent = nullptr);

    // 設置頁面
    void setInstrumentSetup(InstrumentSetupPage* page);
    void setTestConditions(TestConditionsPage* page);
    void setManualControl(ManualControlPage* page);
    void setCommandConsole(CommandConsolePage* page);
    void setTestSequence(TestSequencePage* page);
    void setWaveformOcr(WaveformOcrPage* page);
    void setWfmViewer(WfmViewerPage* page);

    // 取得 TabWidget（如需要外部存取）
    QTabWidget* tabWidget() const { return m_tabWidget; }

    // 執行中鎖定其他 tab，只保留 keepIndex 可點擊
    void setOtherTabsLocked(bool locked, int keepIndex);

  private:
    QTabWidget* m_tabWidget = nullptr;

    // 頁面指標
    InstrumentSetupPage* m_instrumentSetup = nullptr;
    TestConditionsPage* m_testConditions = nullptr;
    ManualControlPage* m_manualControl = nullptr;
    CommandConsolePage* m_commandConsole = nullptr;
    TestSequencePage* m_testSequence = nullptr;
    WaveformOcrPage* m_waveformOcr = nullptr;
    WfmViewerPage* m_wfmViewer = nullptr;

    void setupUI();

    template <typename T>
    void setPageInternal(T*& currentPage, T* newPage, const QString& iconPath, const QString& title);
};
