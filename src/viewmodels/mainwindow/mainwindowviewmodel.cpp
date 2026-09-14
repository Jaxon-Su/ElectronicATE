#include "mainwindowviewmodel.h"
#include "mainwindowmodel.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "page4viewmodel.h"
#include "page5viewmodel.h"
#include "xmlconfigstore.h"
#include "messageservice.h"
#include "pageconnectioncoordinator.h"
#include <QDebug>
#include <QCoreApplication>
#include <QPointer>

MainWindowViewModel::MainWindowViewModel(MainWindowModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_page1ViewModel(nullptr)
    , m_page2ViewModel(nullptr)
    , m_page3ViewModel(nullptr)
    , m_page4ViewModel(nullptr)
    , m_page5ViewModel(nullptr)
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

    m_page1ViewModel = new Page1ViewModel(m_model->page1Model(), this);
    const QString catalogPath = QCoreApplication::applicationDirPath() + "/XML/Instrument.xml";
    if (!m_model->page1Model()->loadBaseXml(catalogPath))
        qWarning() << "Failed to load instrument catalog:" << catalogPath;
    m_page2ViewModel = new Page2ViewModel(m_model->page2Model(), this);
    m_page3ViewModel = new Page3ViewModel(m_model->page3Model(), this);
    m_page4ViewModel = new Page4ViewModel(m_model->page4Model(), this);
    m_page5ViewModel = new Page5ViewModel(m_model->page5Model(), this);
}

void MainWindowViewModel::setupPageConnections()
{
    PageConnectionCoordinator::setupPageConnections(
        m_page1ViewModel,
        m_page2ViewModel,
        m_page3ViewModel,
        m_page5ViewModel
        );
}

QList<IXmlSerializable*> MainWindowViewModel::xmlPages() const
{
    return { m_page1ViewModel, m_page2ViewModel,
             m_page3ViewModel, m_page4ViewModel, m_page5ViewModel };
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

void MainWindowViewModel::saveConfigAs()
{
    emit requestSaveDialog();
}

void MainWindowViewModel::loadConfig()
{
    emit requestLoadDialog();
}

void MainWindowViewModel::onSaveDialogAccepted(const QString& fileName)
{
    if (fileName.isEmpty()) return;

    QString finalFileName = fileName;
    if (!finalFileName.endsWith(".xml", Qt::CaseInsensitive)) {
        finalFileName += ".xml";
    }

    const auto result = XmlConfigStore::saveAllToXml(finalFileName, xmlPages());
    if (reportXmlResult(result, finalFileName, true))
        m_model->setLastSavePath(finalFileName);
}

void MainWindowViewModel::onLoadDialogAccepted(const QString& fileName)
{
    if (fileName.isEmpty()) return;

    if (m_page3ViewModel->isControlActive() || m_page4ViewModel->isControlActive() || m_page5ViewModel->isRunning()) {
        MessageService::instance().showWarning(tr("無法載入"), tr("請先停止控制並斷線，再載入設定。"));
        return;
    }
    QPointer<MainWindowViewModel> alive(this);
    const auto result = XmlConfigStore::loadAllFromXml(fileName, xmlPages(), [this] {
        const auto config = m_page1ViewModel->currentConfig();
        m_page2ViewModel->onPage1ConfigChanged(config);
        m_page2ViewModel->setMaxOutput(config.loadOutputs);
        m_page2ViewModel->setMaxRelayOutput(config.relayOutputs);
        m_page3ViewModel->onPage1ConfigChanged(config);
        m_page3ViewModel->onConditionsChanged(m_page2ViewModel->conditions());
        m_page5ViewModel->onPage1ConfigChanged(config);
    });
    if (!alive) return;
    if (reportXmlResult(result, fileName, false))
        m_model->setLastSavePath(fileName);
}

bool MainWindowViewModel::reportXmlResult(
    const XmlOperationResult& result, const QString& fileName, bool saving)
{
    if (result.succeeded()) return true;

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
