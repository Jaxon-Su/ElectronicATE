#include "mainwindowmodel.h"
#include "page1model.h"
#include "page2model.h"
#include "page3model.h"
#include "page4model.h"
#include "page5model.h"

MainWindowModel::MainWindowModel(QObject* parent)
    : QObject(parent)
    , m_page1Model(nullptr)
    , m_page2Model(nullptr)
    , m_page3Model(nullptr)
    , m_page4Model(nullptr)
    , m_page5Model(nullptr)
{
    initializeSubModels();
}

MainWindowModel::~MainWindowModel()
{

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
    m_page4Model = new Page4Model(this);
    m_page5Model = new Page5Model(this);
}
