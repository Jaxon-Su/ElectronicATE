#pragma once

#include <QObject>
#include <QString>

class InstrumentSetupModel;
class TestConditionsModel;
class ManualControlModel;
class CommandConsoleModel;
class TestSequenceModel;

class MainWindowModel : public QObject {
    Q_OBJECT

  public:
    explicit MainWindowModel(QObject* parent = nullptr);
    ~MainWindowModel();

    // 檔案路徑管理
    const QString& lastSavePath() const { return m_lastSavePath; }
    void setLastSavePath(const QString& path);

    // 子模組存取
    InstrumentSetupModel* instrumentSetupModel() const { return m_instrumentSetupModel; }
    TestConditionsModel* testConditionsModel() const { return m_testConditionsModel; }
    ManualControlModel* manualControlModel() const { return m_manualControlModel; }
    CommandConsoleModel* commandConsoleModel() const { return m_commandConsoleModel; }
    TestSequenceModel* testSequenceModel() const { return m_testSequenceModel; }

  private:
    // 檔案路徑
    QString m_lastSavePath;

    // 子模組 - 使用 nullptr 初始化
    InstrumentSetupModel* m_instrumentSetupModel = nullptr;
    TestConditionsModel* m_testConditionsModel = nullptr;
    ManualControlModel* m_manualControlModel = nullptr;
    CommandConsoleModel* m_commandConsoleModel = nullptr;
    TestSequenceModel* m_testSequenceModel = nullptr;

    void initializeSubModels();
};
