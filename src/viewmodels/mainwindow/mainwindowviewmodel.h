#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "ixmlserializable.h"

class MainWindowModel;
class InstrumentSetupViewModel;
class TestConditionsViewModel;
class ManualControlViewModel;
class CommandConsoleViewModel;
class TestSequenceViewModel;
class WaveformOcrViewModel;
struct XmlOperationResult;

class MainWindowViewModel : public QObject
{
    Q_OBJECT

  public:
    explicit MainWindowViewModel(MainWindowModel *model, QObject *parent = nullptr);
    ~MainWindowViewModel();

    // Getter
    InstrumentSetupViewModel *instrumentSetupViewModel() const { return m_instrumentSetupViewModel; }
    TestConditionsViewModel *testConditionsViewModel() const { return m_testConditionsViewModel; }
    ManualControlViewModel *manualControlViewModel() const { return m_manualControlViewModel; }
    CommandConsoleViewModel *commandConsoleViewModel() const { return m_commandConsoleViewModel; }
    TestSequenceViewModel *testSequenceViewModel() const { return m_testSequenceViewModel; }
    WaveformOcrViewModel *waveformOcrViewModel() const { return m_waveformOcrViewModel; }

    // 檔案操作
    void saveConfig();
    void saveConfigAs();
    void loadConfig();
    // Reapply the existing interlocks after cancelling a window-close attempt.
    void updateControlPermissions();

  signals:
    void controlStateChanged();
    void requestSaveDialog();
    void requestLoadDialog();
    void showMessage(const QString &title, const QString &message, int type);

  public slots:
    void onSaveDialogAccepted(const QString &fileName);
    void onLoadDialogAccepted(const QString &fileName);

  private:
    MainWindowModel *m_model;

    // 子 ViewModels
    InstrumentSetupViewModel *m_instrumentSetupViewModel;
    TestConditionsViewModel *m_testConditionsViewModel;
    ManualControlViewModel *m_manualControlViewModel;
    CommandConsoleViewModel *m_commandConsoleViewModel;
    TestSequenceViewModel *m_testSequenceViewModel;
    WaveformOcrViewModel *m_waveformOcrViewModel = nullptr;

    void initializeViewModels();
    void setupPageConnections();
    QList<IXmlSerializable *> xmlPages() const;
    bool reportXmlResult(const XmlOperationResult &result, const QString &fileName, bool saving);
};
