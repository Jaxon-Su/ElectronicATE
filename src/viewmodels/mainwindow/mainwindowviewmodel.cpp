#include "mainwindowviewmodel.h"
#include "mainwindowmodel.h"
#include "tablewidget.h"
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page4.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "page4viewmodel.h"
#include "appservice.h"
#include <QDebug>

MainWindowViewModel::MainWindowViewModel(MainWindowModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_mainWidget(nullptr)
    , m_page1(nullptr)
    , m_page2(nullptr)
    , m_page3(nullptr)
    , m_page4(nullptr)
    , m_page1ViewModel(nullptr)
    , m_page2ViewModel(nullptr)
    , m_page3ViewModel(nullptr)
    , m_page4ViewModel(nullptr)
{
    initializeViewModels();
    initializePages();
    initializeMainWidget();
    setupPageConnections();

    // 註冊 Meta Types
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
}

void MainWindowViewModel::initializePages()
{
    m_page1 = new Page1(m_page1ViewModel);
    m_page2 = new Page2(m_page2ViewModel);
    m_page3 = new Page3(m_page3ViewModel);
    m_page4 = new Page4(m_page4ViewModel);
}

void MainWindowViewModel::initializeMainWidget()
{
    m_mainWidget = new TableWidget();
    m_mainWidget->setPage1(m_page1);
    m_mainWidget->setPage2(m_page2);
    m_mainWidget->setPage3(m_page3);
    // m_mainWidget->setPage4(m_page4);
}

void MainWindowViewModel::setupPageConnections()
{
    AppService::instance().setupPageConnections(
        m_page1ViewModel,
        m_page2ViewModel,
        m_page3ViewModel
        );
}

void MainWindowViewModel::saveConfig()
{
    if (m_model->lastSavePath().isEmpty()) {
        saveConfigAs();
        return;
    }

    AppService::instance().saveAllToXml(
        m_model->lastSavePath(),
        m_page1, m_page2, m_page3,
        m_page1ViewModel, m_page2ViewModel, m_page3ViewModel
        );
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

    AppService::instance().saveAllToXml(
        finalFileName,
        m_page1, m_page2, m_page3,
        m_page1ViewModel, m_page2ViewModel, m_page3ViewModel
        );

    m_model->setLastSavePath(finalFileName);
}

void MainWindowViewModel::onLoadDialogAccepted(const QString& fileName)
{
    if (fileName.isEmpty()) return;

    AppService::instance().loadAllFromXml(
        fileName,
        m_page1ViewModel, m_page2ViewModel, m_page3ViewModel
        );

    m_model->setLastSavePath(fileName);
}
