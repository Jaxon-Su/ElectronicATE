#include "mainwindowviewmodel.h"
#include "mainwindowmodel.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "page4viewmodel.h"
#include "page5viewmodel.h"
#include "appservice.h"
#include <QDebug>

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
    AppService::instance().registerMetaTypes();
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
    m_page2ViewModel = new Page2ViewModel(m_model->page2Model(), this);
    m_page3ViewModel = new Page3ViewModel(m_model->page3Model(), this);
    m_page4ViewModel = new Page4ViewModel(m_model->page4Model(), this);
    m_page5ViewModel = new Page5ViewModel(m_model->page5Model(), this);
}

void MainWindowViewModel::setupPageConnections()
{
    AppService::instance().setupPageConnections(
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
    AppService::instance().saveAllToXml(m_model->lastSavePath(), xmlPages());
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

    AppService::instance().saveAllToXml(finalFileName, xmlPages());

    m_model->setLastSavePath(finalFileName);
}

void MainWindowViewModel::onLoadDialogAccepted(const QString& fileName)
{
    if (fileName.isEmpty()) return;

    AppService::instance().loadAllFromXml(fileName, xmlPages());

    m_model->setLastSavePath(fileName);
}
