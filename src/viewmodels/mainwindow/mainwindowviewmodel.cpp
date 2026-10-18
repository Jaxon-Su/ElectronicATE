#include "../../infrastructure/worker/testrunsession.h"
#include "mainwindowviewmodel.h"
#include "mainwindowmodel.h"
#include "instrumentsetupviewmodel.h"
#include "testconditionsviewmodel.h"
#include "manualcontrolviewmodel.h"
#include "commandconsoleviewmodel.h"
#include "testsequenceviewmodel.h"
#include "waveformocrviewmodel.h"
#include "../wfmviewer/wfmviewerviewmodel.h"
#include "xmlconfigstore.h"
#include "messageservice.h"
#include "pageconnectioncoordinator.h"
#include <QDebug>
#include <QCoreApplication>
#include <QPointer>

MainWindowViewModel::MainWindowViewModel(MainWindowModel *model, QObject *parent)
    : QObject(parent), m_model(model), m_instrumentSetupViewModel(nullptr), m_testConditionsViewModel(nullptr),
      m_manualControlViewModel(nullptr), m_commandConsoleViewModel(nullptr), m_testSequenceViewModel(nullptr)
{
    initializeViewModels();
    setupPageConnections();
    qRegisterMetaType<TableKind>("TableKind");
}

MainWindowViewModel::~MainWindowViewModel()
{
    // Qt 父子關係會自動清理
}

void MainWindowViewModel::initializeViewModels()
{
    if (!m_model) {
        qWarning() << "MainWindowViewModel: model is null";
        return;
    }

    m_instrumentSetupViewModel = new InstrumentSetupViewModel(m_model->instrumentSetupModel(), this);
    const QString catalogPath = QCoreApplication::applicationDirPath() + "/XML/Instrument.xml";
    if (!m_model->instrumentSetupModel()->loadBaseXml(catalogPath))
        qWarning() << "Failed to load instrument catalog:" << catalogPath;
    m_testConditionsViewModel = new TestConditionsViewModel(m_model->testConditionsModel(), this);
    m_manualControlViewModel = new ManualControlViewModel(m_model->manualControlModel(), this);
    m_commandConsoleViewModel = new CommandConsoleViewModel(m_model->commandConsoleModel(), this);
    m_testSequenceViewModel = new TestSequenceViewModel(m_model->testSequenceModel(), makeTestRunSession(), this);
    m_waveformOcrViewModel = new WaveformOcrViewModel(this);
    m_wfmViewerViewModel = new WfmViewerViewModel(this);
}

void MainWindowViewModel::setupPageConnections()
{
    PageConnectionCoordinator::setupPageConnections(m_instrumentSetupViewModel, m_testConditionsViewModel, m_manualControlViewModel,
                                                    m_testSequenceViewModel);
    connect(m_manualControlViewModel, &ManualControlViewModel::controlActiveChanged, this,
            &MainWindowViewModel::updateControlPermissions);
    connect(m_commandConsoleViewModel, &CommandConsoleViewModel::controlActiveChanged, this,
            &MainWindowViewModel::updateControlPermissions);
    connect(m_testSequenceViewModel, &TestSequenceViewModel::runningChanged, this,
            &MainWindowViewModel::updateControlPermissions);
    updateControlPermissions();
}

void MainWindowViewModel::updateControlPermissions()
{
    const bool manual = m_manualControlViewModel->hasActiveControl();
    const bool console = m_commandConsoleViewModel->isControlActive();
    const bool test = m_testSequenceViewModel->isRunning();
    const bool connecting = m_manualControlViewModel->isConfigurationBusy();
    m_manualControlViewModel->setControlAllowed(!console && !test);
    m_commandConsoleViewModel->setControlAllowed(!manual && !test && !connecting);
    m_testSequenceViewModel->setControlAllowed(!manual && !console && !connecting);
    emit controlStateChanged();
}

QList<IXmlSerializable *> MainWindowViewModel::xmlPages() const
{
    return {m_instrumentSetupViewModel, m_testConditionsViewModel, m_manualControlViewModel, m_commandConsoleViewModel, m_testSequenceViewModel};
}

void MainWindowViewModel::saveConfig()
{
    if (m_model->lastSavePath().isEmpty()) {
        saveConfigAs();
        return;
    }
    const auto result = XmlConfigStore::saveAllToXml(m_model->lastSavePath(), xmlPages());
    reportXmlResult(result, m_model->lastSavePath(), true);
}

void MainWindowViewModel::saveConfigAs() { emit requestSaveDialog(); }

void MainWindowViewModel::loadConfig() { emit requestLoadDialog(); }

void MainWindowViewModel::onSaveDialogAccepted(const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    QString finalFileName = fileName;
    if (!finalFileName.endsWith(".xml", Qt::CaseInsensitive)) {
        finalFileName += ".xml";
    }

    const auto result = XmlConfigStore::saveAllToXml(finalFileName, xmlPages());
    if (reportXmlResult(result, finalFileName, true))
        m_model->setLastSavePath(finalFileName);
}

void MainWindowViewModel::onLoadDialogAccepted(const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    if (m_manualControlViewModel->isControlActive() || m_commandConsoleViewModel->isControlActive() ||
        m_testSequenceViewModel->isRunning()) {
        MessageService::instance().showWarning(tr("無法載入"), tr("請先停止控制並斷線，再載入設定。"));
        return;
    }
    QPointer<MainWindowViewModel> alive(this);
    const auto result = XmlConfigStore::loadAllFromXml(fileName, xmlPages(), [this] {
        const auto config = m_instrumentSetupViewModel->currentConfig();
        m_testConditionsViewModel->onInstrumentSetupConfigChanged(config);
        m_testConditionsViewModel->setMaxOutput(config.loadOutputs);
        m_testConditionsViewModel->setMaxRelayOutput(config.relayOutputs);
        m_manualControlViewModel->onInstrumentSetupConfigChanged(config);
        m_manualControlViewModel->onConditionsChanged(m_testConditionsViewModel->conditions());
        m_testSequenceViewModel->onInstrumentSetupConfigChanged(config);
    });
    if (!alive)
        return;
    if (reportXmlResult(result, fileName, false))
        m_model->setLastSavePath(fileName);
}

bool MainWindowViewModel::reportXmlResult(const XmlOperationResult &result, const QString &fileName,
                                          bool saving)
{
    if (result.succeeded())
        return true;

    QString title = saving ? tr("儲存失敗") : tr("載入失敗");
    QString message;
    switch (result.error) {
    case XmlOperationResult::Error::Open:
        message = tr("無法開啟檔案: %1\n%2").arg(fileName, result.detail);
        break;
    case XmlOperationResult::Error::Write:
        message = tr("無法寫入檔案: %1\n%2").arg(fileName, result.detail);
        break;
    case XmlOperationResult::Error::Parse:
        title = tr("XML 解析錯誤");
        message = tr("行 %1: %2").arg(result.line).arg(result.detail);
        break;
    case XmlOperationResult::Error::InvalidRoot:
        title = tr("XML 格式錯誤");
        message = tr("預期根元素 loodGUI，實際為 %1。").arg(result.detail);
        break;
    case XmlOperationResult::Error::None:
        return true;
    }
    MessageService::instance().showError(title, message);
    return false;
}
