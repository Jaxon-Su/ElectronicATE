#include "mainwindowmodel.h"
#include "page1model.h"
#include "page2model.h"
#include "page3model.h"

MainWindowModel::MainWindowModel(QObject* parent)
    : QObject(parent)
    , m_page1Model(nullptr)
    , m_page2Model(nullptr)
    , m_page3Model(nullptr)
{
    initializeSubModels();
}

MainWindowModel::~MainWindowModel()
{
    // Qt 父子關係會自動清理
}

void MainWindowModel::setLastSavePath(const QString& path)
{
    if (m_lastSavePath != path) {
        m_lastSavePath = path;
    }
}

void MainWindowModel::initializeSubModels()
{
    m_page1Model = new Page1Model(this);
    m_page2Model = new Page2Model(this);
    m_page3Model = new Page3Model(this);
}
