#include "page3viewmodel.h"
#include <QDebug>
#include "messageservice.h"
#include "TriggerControllerFactory.h"
#include <QtConcurrent/QtConcurrentRun>
#include "communicationfactory.h"
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include "oscilloscopefactory.h"
#include "instrumentconfigvalidator.h"
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSettings>
#include "pngcapturecommand.h"
#include "csvcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "savedirpreference.h"
#include "oscilloscopemanager.h"

Page3ViewModel::Page3ViewModel(Page3Model* p3, QObject *parent)
    : QObject(parent), m_model(p3)
    , m_captureInProgress(std::make_shared<std::atomic<bool>>(false))
{
    m_debounce = new Debounce(configDelayTime, this);
    connect(m_debounce, &Debounce::fired, this, &Page3ViewModel::applyPendingConfig);
}

Page3ViewModel::~Page3ViewModel()
{
    // 等待非同步擷取作業完成（最多 2 秒）
    // CaptureCommand lambda 持有 shared_ptr<atomic<bool>>，析構時會重置旗標
    // 此處僅需等待 lambda 確實退出，不會有懸空指標問題
    int waitMs = 0;
    while (m_captureInProgress->load() && waitMs < 2000) {
        QThread::msleep(10);
        waitMs += 10;
    }

    m_debounce->cancel();
    cleanupTriggerResources();
    cleanupAllInstruments();
}

void Page3ViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0) return;
    emit headersChanged(TableHeaderBuilder::buildIndexHeaders(maxOutput));
}

void Page3ViewModel::setNameList(const QStringList &names)
{
    emit rowLabelsChanged(names);
}

void Page3ViewModel::updateTitles(TableKind type, const QStringList& titles)
{
    switch (type) {
    case TableKind::Input:
        if (m_model) m_model->setInputTitles(titles);
        m_inputTitles = titles;
        emit titlesUpdated(TableKind::Input, titles);
        break;
    case TableKind::Load:
        if (m_model) m_model->setLoadTitles(titles);
        m_loadTitles = titles;
        emit titlesUpdated(TableKind::Load, titles);
        break;
    case TableKind::DyLoad:
        if (m_model) m_model->setDyLoadTitles(titles);
        m_dyloadTitles = titles;
        emit titlesUpdated(TableKind::DyLoad, titles);
        break;
    case TableKind::Relay:
        if (m_model) m_model->setRelayTitles(titles);
        m_relayTitles = titles;
        emit titlesUpdated(TableKind::Relay, titles);
        break;
    case TableKind::Dc:
        break;
    }
}

void Page3ViewModel::createAllInstruments()
{
    cleanupAllInstruments();
    auto map = OscilloscopeManager::buildFromConfig(m_page1Config);
    m_oscManager.assign(std::move(map));
}

void Page3ViewModel::cleanupAllInstruments()
{
    m_oscManager.clear(m_currentTriggerController);
}

void Page3ViewModel::onPage1ConfigChanged(const Page1Config &cfg)
{
    QMutexLocker locker(&m_stateMutex);

    // 儲存最新配置
    m_pendingConfig = cfg;

    switch (m_updateState) {
    case ConfigUpdateState::Idle:
        m_updateState = ConfigUpdateState::Pending;
        m_debounce->schedule();
        break;
    case ConfigUpdateState::Pending:
        m_debounce->schedule();   // 重置倒數
        break;
    case ConfigUpdateState::Processing:
        m_updateState = ConfigUpdateState::Pending;
        m_debounce->schedule();
        break;
    }
}

void Page3ViewModel::applyPendingConfig()
{
    // ===== 狀態檢查與轉換 =====
    {
        QMutexLocker locker(&m_stateMutex);

        if (m_updateState != ConfigUpdateState::Pending) {
            return;
        }

        // 切換到處理中狀態
        m_updateState = ConfigUpdateState::Processing;
    }

    // ===== 執行配置更新 =====
    m_page1Config = m_pendingConfig;

    // 狀態先回 Idle
    {
        QMutexLocker locker(&m_stateMutex);
        m_updateState = ConfigUpdateState::Idle;
    }

    // 發出配置已變更信號（UI 相關，須在主執行緒）
    emit page1ConfigChanged(m_pendingConfig);

    // 若擷取作業正在進行，延後建立連線（1 秒後重試）
    if (m_captureInProgress->load()) {
        qDebug() << "[Page3VM] Capture in progress, deferring oscilloscope reconnect";
        QMutexLocker locker(&m_stateMutex);
        m_updateState = ConfigUpdateState::Pending;
        QTimer::singleShot(1000, this, [this]() { m_debounce->schedule(); });
        return;
    }

        if (m_isConnecting) {
        qDebug() << "[Page3VM] Connection already in progress, skipping";
        return;
    }
    m_isConnecting = true;

    // 取出舊 map 轉移給背景執行緒負責 disconnect
    OscilloscopeManager::OscMap oldMap;
    {
        // 直接從 m_oscManager 取出舊 map，清空 manager 狀態
        m_oscManager.clear(m_currentTriggerController);
    }

    Page1Config configSnapshot = m_page1Config;
    QPointer<Page3ViewModel> self(this);

    QtConcurrent::run([self, configSnapshot]() mutable {
        // 建立新示波器（在背景執行緒）
        auto newMap = OscilloscopeManager::buildFromConfig(configSnapshot);

        QMetaObject::invokeMethod(self, [self, newMap = std::move(newMap)]() mutable {
            if (!self) return;

            self->m_oscManager.assign(std::move(newMap));
            self->m_isConnecting = false;

            qDebug() << "[Page3VM] Oscilloscopes ready:" << self->m_oscManager.modelNames();

            if (self->m_currentTriggerController && self->m_oscManager.current()) {
                self->connectTriggerController();
            }
        }, Qt::QueuedConnection);
    });
}

void Page3ViewModel::onInputDataChanged(const QVector<InputRow>& rows) {
    m_page2InputData = rows;
}

void Page3ViewModel::onLoadMetaChanged(const LoadMetaRow& meta) {
    if (m_model) m_model->setPage2LoadMetaDataChanged(meta);
    m_LoadMetaData = meta;
}

void Page3ViewModel::onLoadRowsChanged(const QVector<LoadDataRow>& rows) {
    if (m_model) m_model->setPage2LoadRowsChanged(rows);
    m_LoadRowsData = rows;
}

void Page3ViewModel::onRelayRowsChanged(const QVector<RelayDataRow>& rows) {
    if (m_model) m_model->setPage2RelayRowsChanged(rows);
    m_RelayRowsData = rows;

    QStringList titles;
    for(const auto& row : std::as_const(rows)) {
        titles << row.label;
    }
    m_relayTitles = titles;
    emit titlesUpdated(TableKind::Relay, titles);
}

void Page3ViewModel::onDynamicMetaChanged(const DynamicMetaRow& meta) {
    if (m_model) m_model->setPage2DynamicMetaChanged(meta);
    m_DynamicMetaData = meta;
}

void Page3ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow>& rows) {
    if (m_model) m_model->setPage2DynamicRowsChanged(rows);
    m_DynamicRowsData = rows;
}

void Page3ViewModel::onInputToggled(bool on)
{
    if (on){
        handleInput(InputAction::PowerOn);
    }
    else{
        handleInput(InputAction::PowerOff);
    }
}

void Page3ViewModel::onInputChanged()
{
    handleInput(InputAction::Change);
}

void Page3ViewModel::onLoadToggled(bool on)
{
    if (on) {
        handleLoad(LoadAction::LoadOn);
    } else {
        handleLoad(LoadAction::LoadOff);
    }
}

void Page3ViewModel::onLoadChanged()
{
    handleLoad(LoadAction::Change);
}

void Page3ViewModel::onDyloadToggled(bool on)
{
    if (on) {
        handleDyLoad(DyLoadAction::DyLoadOn);
    } else {
        handleDyLoad(DyLoadAction::DyloadOff);
    }
}

void Page3ViewModel::onDyLoadChanged()
{
    handleDyLoad(DyLoadAction::Change);
}

void Page3ViewModel::onRelayToggled(bool on)
{
    if (on) {
        handleRelay(RelayAction::RelayOn);
    } else {
        handleRelay(RelayAction::RelayOff);
    }
}

void Page3ViewModel::onRelayChanged()
{
    handleRelay(RelayAction::Change);
}

bool Page3ViewModel::tryBeginLoadOperation(const char* context)
{
    if (m_loadOperationBusy) {
        qWarning() << "[Page3ViewModel]" << context
                   << "ignored because a load operation is still running";
        emit loadOperationBusyChanged(true);
        return false;
    }

    m_loadOperationBusy = true;
    emit loadOperationBusyChanged(true);
    return true;
}

void Page3ViewModel::finishLoadOperation()
{
    m_loadOperationBusy = false;
    emit loadOperationBusyChanged(false);
}

void Page3ViewModel::onSelected(TableKind type, int idx, const QString& txt)
{
    m_selections[type] = { idx, txt };
}

void Page3ViewModel::onTriggerWidgetCreated(const QString& modelName, QObject* triggerController)
{
    cleanupTriggerResources();

    if (auto* ctrl = qobject_cast<AbstractTriggerController*>(triggerController)) {
        m_currentTriggerController = ctrl;
        m_triggerModelName = modelName;

        const bool familyMatch =
            TriggerControllerFactory::getControllerFamily(ctrl->getSupportedModel()) ==
            TriggerControllerFactory::getControllerFamily(modelName);
        if (familyMatch) {
            connectTriggerController();
        } else {
            qWarning() << "[Page3ViewModel] Controller model mismatch:"
                       << ctrl->getSupportedModel() << "vs" << modelName;
        }
    }
}

void Page3ViewModel::onTriggerWidgetDestroyed()
{
    cleanupTriggerResources();
}

void Page3ViewModel::connectTriggerController()
{
    if (!m_currentTriggerController) {
        qWarning() << "[Page3ViewModel] No trigger controller available";
        return;
    }

    // 每次重新連線前先斷舊連線，避免重複 connect()
    disconnect(m_currentTriggerController, &AbstractTriggerController::reconnectRequested,
               this, &Page3ViewModel::reconnectOscilloscopes);
    connect(m_currentTriggerController, &AbstractTriggerController::reconnectRequested,
            this, &Page3ViewModel::reconnectOscilloscopes,
            Qt::QueuedConnection);

    auto osc = m_oscManager.get(m_triggerModelName);
    if (!osc) {
        qWarning() << "[Page3ViewModel] Oscilloscope not found for model:" << m_triggerModelName;
        m_currentTriggerController->setInstrument(nullptr);
        return;
    }

    m_oscManager.setCurrent(m_triggerModelName);
    m_currentTriggerController->setInstrument(osc.get());
}

void Page3ViewModel::reconnectOscilloscopes()
{
    qDebug() << "[Page3VM] Oscilloscope reconnect requested";
    // 複用現有 Page1 設定重建示波器連線（不需要使用者改動 Page1）
    onPage1ConfigChanged(m_page1Config);
}

void Page3ViewModel::cleanupTriggerResources()
{
    m_currentTriggerController = nullptr;
    m_triggerModelName.clear();
}

void Page3ViewModel::writeXml(QXmlStreamWriter& writer) const
{
    const auto& sel = m_selections;
    m_model->setSelectedInputState  (sel.value(TableKind::Input).index,  sel.value(TableKind::Input).text);
    m_model->setSelectedLoadState   (sel.value(TableKind::Load).index,   sel.value(TableKind::Load).text);
    m_model->setSelectedDyLoadState (sel.value(TableKind::DyLoad).index, sel.value(TableKind::DyLoad).text);
    m_model->setSelectedRelayState  (sel.value(TableKind::Relay).index,  sel.value(TableKind::Relay).text);
    m_model->writeXml(writer);
}

void Page3ViewModel::loadXml(QXmlStreamReader& reader)
{
    m_model->loadXml(reader);
    restoreFromModel();
    updateUIAfterLoad();
}

void Page3ViewModel::restoreFromModel()
{
    if (!m_model) return;

    m_inputTitles = m_model->getInputTitles();
    m_loadTitles = m_model->getLoadTitles();
    m_dyloadTitles = m_model->getDyLoadTitles();
    m_relayTitles = m_model->getRelayTitles();

    m_LoadMetaData = m_model->getLoadMetaData();
    m_LoadRowsData = m_model->getLoadRowsData();
    {
        auto newMeta = m_model->getDynamicMetaData();
        // 舊格式 XML 的 Page3 段落不含 T1T2，此時保留 Page2 信號已送來的值
        if (newMeta.t1t2.isEmpty() && !m_DynamicMetaData.t1t2.isEmpty())
            newMeta.t1t2 = m_DynamicMetaData.t1t2;
        m_DynamicMetaData = newMeta;
    }
    m_DynamicRowsData = m_model->getDynamicRowsData();
    m_RelayRowsData = m_model->getRelayRowsData();

    m_selections[TableKind::Input]  = { m_model->getSelectedInputIndex(),   m_model->getSelectedInputText()   };
    m_selections[TableKind::Load]   = { m_model->getSelectedLoadIndex(),    m_model->getSelectedLoadText()    };
    m_selections[TableKind::DyLoad] = { m_model->getSelectedDyLoadIndex(),  m_model->getSelectedDyLoadText()  };
    m_selections[TableKind::Relay]  = { m_model->getSelectedRelayIndex(),   m_model->getSelectedRelayText()   };
}

void Page3ViewModel::updateUIAfterLoad()
{
    emit titlesUpdated(TableKind::Input, m_inputTitles);
    emit titlesUpdated(TableKind::Load, m_loadTitles);
    emit titlesUpdated(TableKind::DyLoad, m_dyloadTitles);
    emit titlesUpdated(TableKind::Relay, m_relayTitles);

    QTimer::singleShot(100, this, [this]() {
        restoreUISelections();
    });
}

void Page3ViewModel::restoreUISelections()
{
    for (auto it = m_selections.constBegin(); it != m_selections.constEnd(); ++it) {
        if (it.value().index >= 0)
            emit restoreSelections(it.key(), it.value().index, it.value().text);
    }
}

// handleInput
void Page3ViewModel::handleInput(InputAction action)
{
    const auto& inputSel = m_selections.value(TableKind::Input);
    auto validResult = InstrumentConfigValidator::validateInput(m_page1Config, inputSel.text);
    if (!validResult.isValid) {
        MessageService::instance().showWarning(validResult.errorTitle, validResult.errorMessage);
        emit forceOff(TableKind::Input);
        return;
    }

    const Page1Config cfg      = m_page1Config;
    if (inputSel.index < 0 || inputSel.index >= m_page2InputData.size()) {
        MessageService::instance().showWarning(
            "Input Selection Error",
            "Selected input row is out of range.");
        emit forceOff(TableKind::Input);
        return;
    }

    const InputRow inputRow = m_page2InputData.at(inputSel.index);
    QPointer<Page3ViewModel> self(this);

    QtConcurrent::run([cfg, inputRow, action, self]() {
        try {
            auto res = InstrumentExecutor::runInput(cfg, inputRow, action);
            if (!res.success && self && action == InputAction::PowerOn)
                emit self->forceOff(TableKind::Input);
        } catch (const std::exception& ex) {
            qWarning() << "[handleInput] Exception:" << ex.what();
        }
    });
}

// handleRelay
void Page3ViewModel::handleRelay(RelayAction action)
{
    const auto& relaySel = m_selections.value(TableKind::Relay);
    auto validResult = InstrumentConfigValidator::validateRelay(m_page1Config, relaySel.text);
    if (!validResult.isValid) {
        MessageService::instance().showWarning(validResult.errorTitle, validResult.errorMessage);
        emit forceOff(TableKind::Relay);
        return;
    }

    const Page1Config cfg       = m_page1Config;
    const auto        relayRows = m_RelayRowsData;
    const int         condIdx   = relaySel.index;
    QPointer<Page3ViewModel> self(this);

    QtConcurrent::run([cfg, relayRows, condIdx, action, self]() {
        try {
            auto res = InstrumentExecutor::runRelay(cfg, relayRows, condIdx, action);
            if (!res.success && self) {
                if (!res.errorMessage.isEmpty()) {
                    QMetaObject::invokeMethod(
                        &MessageService::instance(), "showWarning",
                        Qt::QueuedConnection,
                        Q_ARG(QString, "Relay Communication Error"),
                        Q_ARG(QString, res.errorMessage));
                }
                if (action == RelayAction::RelayOn)
                    emit self->forceOff(TableKind::Relay);
            }
        } catch (const std::exception& ex) {
            qWarning() << "[handleRelay] Exception:" << ex.what();
        }
    });
}


// handleLoad
void Page3ViewModel::handleLoad(LoadAction action)
{
    const auto& loadSel = m_selections.value(TableKind::Load);
    auto validResult = InstrumentConfigValidator::validateLoad(m_page1Config, loadSel.text);
    if (!validResult.isValid) {
        MessageService::instance().showWarning(validResult.errorTitle, validResult.errorMessage);
        emit forceOff(TableKind::Load);
        return;
    }
    if (!tryBeginLoadOperation("handleLoad"))
        return;

    const Page1Config cfg      = m_page1Config;
    const auto        loadRows = m_LoadRowsData;
    const int         condIdx  = loadSel.index;
    const auto        meta     = m_LoadMetaData;
    const bool        syncEnabled = m_syncEnabled;
    QPointer<Page3ViewModel> self(this);

    QtConcurrent::run([cfg, loadRows, condIdx, meta, action, syncEnabled, self]() {
        try {
            auto res = InstrumentExecutor::runLoad(cfg, loadRows, condIdx, meta, action, syncEnabled);
            if (!res.success && self) {
                if (!res.errorMessage.isEmpty()) {
                    QMetaObject::invokeMethod(
                        &MessageService::instance(), "showWarning",
                        Qt::QueuedConnection,
                        Q_ARG(QString, "Load Configuration Error"),
                        Q_ARG(QString, res.errorMessage));
                }
                if (action == LoadAction::LoadOn)
                    emit self->forceOff(TableKind::Load);
            }
        } catch (const std::exception& ex) {
            qWarning() << "[handleLoad] Exception:" << ex.what();
        }
        if (self) {
            QMetaObject::invokeMethod(self.data(), [self]() {
                if (self)
                    self->finishLoadOperation();
            }, Qt::QueuedConnection);
        }
    });
}


void Page3ViewModel::handleDyLoad(DyLoadAction action)
{
    const auto& dyLoadSel = m_selections.value(TableKind::DyLoad);
    auto validResult = InstrumentConfigValidator::validateDyLoad(m_page1Config, dyLoadSel.text);
    if (!validResult.isValid) {
        MessageService::instance().showWarning(validResult.errorTitle, validResult.errorMessage);
        emit forceOff(TableKind::DyLoad);
        return;
    }
    if (!tryBeginLoadOperation("handleDyLoad"))
        return;

    const bool syncEnabled = m_syncEnabled;
    const bool syncDirty   = m_syncDirty;
    if (syncEnabled || syncDirty) m_syncDirty = false;

    const Page1Config cfg     = m_page1Config;
    const auto        dyRows  = m_DynamicRowsData;
    const int         condIdx = dyLoadSel.index;
    const auto        meta   = m_DynamicMetaData;
    QPointer<Page3ViewModel> self(this);

    QtConcurrent::run([cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty, self]() {
        try {
            auto res = InstrumentExecutor::runDyLoad(cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty);
            if (!res.success && self) {
                if (!res.errorMessage.isEmpty()) {
                    QMetaObject::invokeMethod(
                        &MessageService::instance(), "showWarning",
                        Qt::QueuedConnection,
                        Q_ARG(QString, "Dynamic Load Configuration Error"),
                        Q_ARG(QString, res.errorMessage));
                }
                if (action == DyLoadAction::DyLoadOn)
                    emit self->forceOff(TableKind::DyLoad);
            }
        } catch (const std::exception& ex) {
            qWarning() << "[handleDyLoad] Exception:" << ex.what();
        }
        if (self) {
            QMetaObject::invokeMethod(self.data(), [self]() {
                if (self)
                    self->finishLoadOperation();
            }, Qt::QueuedConnection);
        }
    });
}

void Page3ViewModel::OnWaveformCaptured()
{
    PngCaptureCommand cmd(buildCaptureContext());
    cmd.execute();
}

void Page3ViewModel::OnCsvCaptured()
{
    CsvCaptureCommand cmd(buildCaptureContext());
    cmd.execute();
}

void Page3ViewModel::OnAllCsvCaptured()
{
    AllCsvCaptureCommand cmd(buildCaptureContext());
    cmd.execute();
}

void Page3ViewModel::OnWfmCaptured()
{
    WfmCaptureCommand cmd(buildCaptureContext());
    cmd.execute();
}

CaptureContext Page3ViewModel::buildCaptureContext() const
{
    CaptureContext ctx;
    ctx.oscilloscope      = m_oscManager.current();
    ctx.captureInProgress = m_captureInProgress;
    ctx.lastSaveDir       = SaveDirPreference::load();
    ctx.onSaveDirChanged  = [](const QString& filePath) {
        SaveDirPreference::save(filePath);
    };
    // 直接讀 trigger widget 的 Source ComboBox，不查詢儀器
    if (m_currentTriggerController)
        ctx.captureChannel = m_currentTriggerController->getSelectedChannel();
    return ctx;
}



void Page3ViewModel::onSyncChanged(bool enabled)
{
    m_syncEnabled = enabled;

    if (!enabled) {
        m_syncDirty = true;
    }

    qDebug() << "[Page3VM] SyncDynamic enabled=" << enabled
             << "dirty=" << m_syncDirty;
}
