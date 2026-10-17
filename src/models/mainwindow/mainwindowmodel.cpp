#include "mainwindowmodel.h"
#include "instrumentsetupmodel.h"
#include "testconditionsmodel.h"
#include "manualcontrolmodel.h"
#include "commandconsolemodel.h"
#include "testsequencemodel.h"

MainWindowModel::MainWindowModel(QObject* parent)
    : QObject(parent), m_instrumentSetupModel(nullptr), m_testConditionsModel(nullptr), m_manualControlModel(nullptr),
      m_commandConsoleModel(nullptr), m_testSequenceModel(nullptr)
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
    m_instrumentSetupModel = new InstrumentSetupModel(this);
    m_testConditionsModel = new TestConditionsModel(this);
    m_manualControlModel = new ManualControlModel(this);
    m_commandConsoleModel = new CommandConsoleModel(this);
    m_testSequenceModel = new TestSequenceModel(this);
}
