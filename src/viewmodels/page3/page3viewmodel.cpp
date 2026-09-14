#include "page3viewmodel.h"
#include <QDebug>
#include <QFutureWatcher>
#include "messageservice.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QPointer>
#include <QTimer>
#include <QScopeGuard>
#include "instrumentconfigvalidator.h"
#include <QDir>
#include "pngcapturecommand.h"
#include "csvcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "savedirpreference.h"
#include "oscilloscopemanager.h"
#include "instrumentoperationrunner.h"
#include "pendingscopes.h"

Page3ViewModel::Page3ViewModel(Page3Model* p3, Page3Operations operations, QObject *parent)
    : QObject(parent), m_model(p3), m_operations(std::move(operations))
    , m_captureSession(std::make_shared<CaptureSession>())
{
    connect(&m_triggerBinding, &TriggerBinding::reconnectRequested,
            this, &Page3ViewModel::reconnectOscilloscopes, Qt::QueuedConnection);
    m_debounce = new Debounce(configDelayTime, this);
    connect(m_debounce, &Debounce::fired, this, &Page3ViewModel::applyPendingConfig);
}

Page3ViewModel::~Page3ViewModel()
{
    m_operationQueue.close();
    m_debounce->cancel();
    cleanupTriggerResources();
    auto scopes = m_oscManager.takeAll();
    m_captureSession->closeWhenIdle([scopes = std::move(scopes)]() mutable {
        OscilloscopeManager::disconnectAll(scopes);
    });
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

void Page3ViewModel::cleanupAllInstruments()
{
    m_triggerBinding.bindInstrument(nullptr);
    m_oscManager.clear();
}

void Page3ViewModel::onPage1ConfigChanged(const Page1Config &cfg)
{
    m_configUpdates.enqueue(cfg);
    m_debounce->schedule();
}

void Page3ViewModel::applyPendingConfig()
{
    if (!m_controlAllowed) return;
    // All queue access is on the ViewModel thread; background work owns a snapshot.
    auto snapshot = m_configUpdates.tryStart(m_captureSession->isBusy());
    if (!snapshot) {
        if (m_configUpdates.hasPending() && !m_configUpdates.isRunning()) {
            QTimer::singleShot(1000, this, [this] { m_debounce->schedule(); });
        }
        return;
    }

    m_page1Config = *snapshot;
    QPointer<Page3ViewModel> alive(this);
    updateControlActivity();
    if (!alive) return;
    emit page1ConfigChanged(m_page1Config);
    if (!alive) return;
    cleanupAllInstruments();
    if (!alive) return;

    // The watcher delivers completion only while this ViewModel is alive.
    // No ViewModel pointer is accessed from the worker thread.
    auto* watcher = new QFutureWatcher<std::shared_ptr<PendingScopes>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<PendingScopes>>::finished,
            this, [this, watcher] {
        QPointer<Page3ViewModel> alive(this);
        QString error;
        try {
            m_oscManager.assign(watcher->result()->take());
            if (m_triggerBinding.hasController() && m_oscManager.current())
                connectTriggerController();
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
        } catch (...) {
            error = tr("Unknown oscilloscope connection error.");
        }
        if (!alive) return;
        watcher->deleteLater();
        m_configUpdates.complete();
        updateControlActivity();
        if (!alive) return;
        if (m_configUpdates.hasPending()) m_debounce->schedule();
        if (!error.isEmpty())
            MessageService::instance().showWarning(tr("Connection Failed"), error);
    });
    watcher->setFuture(QtConcurrent::run([config = std::move(*snapshot), connectScopes = m_operations.connectScopes] {
        return std::make_shared<PendingScopes>(connectScopes(config));
    }));
}
void Page3ViewModel::onInputDataChanged(const QVector<InputRow>& rows) {
    m_conditions.inputRows = rows;
}

void Page3ViewModel::onConditionsChanged(const TestConditionSnapshot& snapshot)
{
    m_conditions = snapshot;
    onInputDataChanged(snapshot.inputRows);
    onLoadMetaChanged(snapshot.loadMeta);
    onLoadRowsChanged(snapshot.loadRows);
    onDynamicMetaChanged(snapshot.dynamicMeta);
    onDynamicRowsChanged(snapshot.dynamicRows);
    // This handler publishes titles; call only after all condition data is set.
    onRelayRowsChanged(snapshot.relayRows);
}

void Page3ViewModel::onLoadMetaChanged(const LoadMetaRow& meta) {
    if (m_model) m_model->setPage2LoadMetaDataChanged(meta);
    m_conditions.loadMeta = meta;
}

void Page3ViewModel::onLoadRowsChanged(const QVector<LoadDataRow>& rows) {
    if (m_model) m_model->setPage2LoadRowsChanged(rows);
    m_conditions.loadRows = rows;
}

void Page3ViewModel::onRelayRowsChanged(const QVector<RelayDataRow>& rows) {
    if (m_model) m_model->setPage2RelayRowsChanged(rows);
    m_conditions.relayRows = rows;

    QStringList titles;
    for(const auto& row : std::as_const(rows)) {
        titles << row.label;
    }
    m_relayTitles = titles;
    emit titlesUpdated(TableKind::Relay, titles);
}

void Page3ViewModel::onDynamicMetaChanged(const DynamicMetaRow& meta) {
    if (m_model) m_model->setPage2DynamicMetaChanged(meta);
    m_conditions.dynamicMeta = meta;
}

void Page3ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow>& rows) {
    if (m_model) m_model->setPage2DynamicRowsChanged(rows);
    m_conditions.dynamicRows = rows;
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
    QPointer<Page3ViewModel> alive(this);
    emit loadOperationBusyChanged(true);
    return !alive.isNull();
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
    auto* controller = qobject_cast<ITriggerController*>(triggerController);
    if (m_triggerBinding.attach(controller, modelName)) {
        connectTriggerController();
    } else if (controller) {
        qWarning() << "[Page3ViewModel] Controller model mismatch:"
                   << controller->getSupportedModel() << "vs" << modelName;
    }
}

void Page3ViewModel::onTriggerWidgetDestroyed()
{
    cleanupTriggerResources();
}

void Page3ViewModel::connectTriggerController()
{
    if (!m_triggerBinding.hasController()) {
        qWarning() << "[Page3ViewModel] No trigger controller available";
        return;
    }
    const QString modelName = m_triggerBinding.modelName();
    auto osc = m_oscManager.get(modelName);
    if (!osc) {
        qWarning() << "[Page3ViewModel] Oscilloscope not found for model:" << modelName;
        m_triggerBinding.bindInstrument(nullptr);
        return;
    }
    m_oscManager.setCurrent(modelName);
    m_triggerBinding.bindInstrument(osc.get());
}

void Page3ViewModel::reconnectOscilloscopes()
{
    qDebug() << "[Page3VM] Oscilloscope reconnect requested";
    // 複用現有 Page1 設定重建示波器連線（不需要使用者改動 Page1）
    onPage1ConfigChanged(m_page1Config);
}

void Page3ViewModel::cleanupTriggerResources()
{
    m_triggerBinding.detach();
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
    if (reader.hasError()) return;
    restoreFromModel();
    if (!signalsBlocked()) updateUIAfterLoad();
}

void Page3ViewModel::restoreFromModel()
{
    if (!m_model) return;

    m_inputTitles = m_model->getInputTitles();
    m_loadTitles = m_model->getLoadTitles();
    m_dyloadTitles = m_model->getDyLoadTitles();
    m_relayTitles = m_model->getRelayTitles();

    m_conditions.loadMeta = m_model->getLoadMetaData();
    m_conditions.loadRows = m_model->getLoadRowsData();
    {
        auto newMeta = m_model->getDynamicMetaData();
        // 舊格式 XML 的 Page3 段落不含 T1T2，此時保留 Page2 信號已送來的值
        if (newMeta.t1t2.isEmpty() && !m_conditions.dynamicMeta.t1t2.isEmpty())
            newMeta.t1t2 = m_conditions.dynamicMeta.t1t2;
        m_conditions.dynamicMeta = newMeta;
    }
    m_conditions.dynamicRows = m_model->getDynamicRowsData();
    m_conditions.relayRows = m_model->getRelayRowsData();

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

void Page3ViewModel::rejectOperation(const QString& title, const QString& message, TableKind type)
{
    QPointer<Page3ViewModel> alive(this);
    MessageService::instance().showWarning(title, message);
    if (alive) emit forceOff(type);
    if (alive) emit restoreOutputState(type, m_outputsOn.value(type));
}

// handleInput
void Page3ViewModel::handleInput(InputAction action)
{
    if (!m_controlAllowed) return;
    const auto& inputSel = m_selections.value(TableKind::Input);
    auto validResult = InstrumentConfigValidator::validateInput(m_page1Config, inputSel.text);
    if (!validResult.isValid) {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Input);
        return;
    }

    const Page1Config cfg      = m_page1Config;
    if (inputSel.index < 0 || inputSel.index >= m_conditions.inputRows.size()) {
        rejectOperation("Input Selection Error", "Selected input row is out of range.", TableKind::Input);
        return;
    }

    const InputRow inputRow = m_conditions.inputRows.at(inputSel.index);
    startInstrumentOperation([cfg, inputRow, action, run = m_operations.input] {
        return run(cfg, inputRow, action);
    }, "Input Configuration Error", TableKind::Input, action == InputAction::PowerOn, false,
        action == InputAction::Change ? std::nullopt : std::optional<bool>(action == InputAction::PowerOn));
}

// handleRelay
void Page3ViewModel::handleRelay(RelayAction action)
{
    if (!m_controlAllowed) return;
    const auto& relaySel = m_selections.value(TableKind::Relay);
    auto validResult = InstrumentConfigValidator::validateRelay(m_page1Config, relaySel.text);
    if (!validResult.isValid) {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Relay);
        return;
    }

    const Page1Config cfg       = m_page1Config;
    const auto        relayRows = m_conditions.relayRows;
    const int         condIdx   = relaySel.index;
    startInstrumentOperation([cfg, relayRows, condIdx, action, run = m_operations.relay] {
        return run(cfg, relayRows, condIdx, action);
    }, "Relay Communication Error", TableKind::Relay, action == RelayAction::RelayOn, false,
        action == RelayAction::Change ? std::nullopt : std::optional<bool>(action == RelayAction::RelayOn));
}


// handleLoad
void Page3ViewModel::handleLoad(LoadAction action)
{
    if (!m_controlAllowed) return;
    const auto& loadSel = m_selections.value(TableKind::Load);
    auto validResult = InstrumentConfigValidator::validateLoad(m_page1Config, loadSel.text);
    if (!validResult.isValid) {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Load);
        return;
    }
    if (!tryBeginLoadOperation("handleLoad"))
        return;

    const Page1Config cfg      = m_page1Config;
    const auto        loadRows = m_conditions.loadRows;
    const int         condIdx  = loadSel.index;
    const auto        meta     = m_conditions.loadMeta;
    const bool        syncEnabled = m_syncEnabled;
    startInstrumentOperation([cfg, loadRows, condIdx, meta, action, syncEnabled, run = m_operations.load] {
        return run(cfg, loadRows, condIdx, meta, action, syncEnabled);
    }, "Load Configuration Error", TableKind::Load, action == LoadAction::LoadOn, true,
        action == LoadAction::Change ? std::nullopt : std::optional<bool>(action == LoadAction::LoadOn));
}


void Page3ViewModel::handleDyLoad(DyLoadAction action)
{
    if (!m_controlAllowed) return;
    const auto& dyLoadSel = m_selections.value(TableKind::DyLoad);
    auto validResult = InstrumentConfigValidator::validateDyLoad(m_page1Config, dyLoadSel.text);
    if (!validResult.isValid) {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::DyLoad);
        return;
    }
    if (!tryBeginLoadOperation("handleDyLoad"))
        return;

    const bool syncEnabled = m_syncEnabled;
    const bool syncDirty   = m_syncDirty;
    if (syncEnabled || syncDirty) m_syncDirty = false;

    const Page1Config cfg     = m_page1Config;
    const auto        dyRows  = m_conditions.dynamicRows;
    const int         condIdx = dyLoadSel.index;
    const auto        meta   = m_conditions.dynamicMeta;
    startInstrumentOperation([cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty, run = m_operations.dynamic] {
        return run(cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty);
    }, "Dynamic Load Configuration Error", TableKind::DyLoad, action == DyLoadAction::DyLoadOn, true,
        action == DyLoadAction::Change ? std::nullopt : std::optional<bool>(action == DyLoadAction::DyLoadOn));
}

void Page3ViewModel::OnWaveformCaptured()
{
    PngCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnCsvCaptured()
{
    CsvCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnAllCsvCaptured()
{
    AllCsvCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnWfmCaptured()
{
    WfmCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

CaptureContext Page3ViewModel::buildCaptureContext() const
{
    CaptureContext ctx;
    ctx.oscilloscope      = m_oscManager.current();
    ctx.captureSession = m_captureSession;
    ctx.lastSaveDir       = SaveDirPreference::load();
    ctx.selectSaveFile    = m_captureFileSelector;
    ctx.onSaveDirChanged  = [](const QString& filePath) {
        SaveDirPreference::save(filePath);
    };
    // 直接讀 trigger widget 的 Source ComboBox，不查詢儀器
    ctx.captureChannel = m_triggerBinding.selectedChannel();
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

void Page3ViewModel::validateXml(QXmlStreamReader& reader) const
{
    Page3Model candidate;
    candidate.loadXml(reader);
}

void Page3ViewModel::startInstrumentOperation(std::function<InstrumentOperationResult()> work,
    const QString& errorTitle, TableKind type, bool forceOffOnFailure, bool releaseLoadBusy, std::optional<bool> outputOn)
{
    QPointer<Page3ViewModel> guard(this);
    // An ON request may partially succeed before its driver reports an error.
    // Keep ownership until an OFF command has been confirmed by readback.
    if (outputOn && *outputOn) m_outputsOn[type] = true;
    ++m_pendingOperations;
    updateControlActivity();
    if (!guard) return;
    m_operationQueue.submit(std::move(work),
        [this, errorTitle, type, forceOffOnFailure, releaseLoadBusy, outputOn](const InstrumentOperationResult& result) {
            QPointer<Page3ViewModel> alive(this);
            if (!result.success) {
                if (!result.errorMessage.isEmpty())
                    MessageService::instance().showWarning(errorTitle, result.errorMessage);
                if (!alive) return;
                if (forceOffOnFailure) emit forceOff(type);
                if (alive && outputOn) emit restoreOutputState(type, m_outputsOn.value(type));
            }
            if (alive && releaseLoadBusy) finishLoadOperation();
            if (!alive) return;
            if (result.success && outputOn) m_outputsOn[type] = *outputOn;
            --m_pendingOperations;
            updateControlActivity();
        });
}

bool Page3ViewModel::isControlActive() const
{
    if (m_pendingOperations || m_capturePreparing || m_captureSession->isBusy() || m_configUpdates.isRunning()) return true;
    for (bool on : m_outputsOn) if (on) return true;
    return false;
}
void Page3ViewModel::setControlAllowed(bool allowed)
{
    if (m_controlAllowed == allowed) return;
    m_controlAllowed = allowed;
    if (allowed && m_configUpdates.hasPending()) m_debounce->schedule();
}
void Page3ViewModel::updateControlActivity()
{
    const bool active = isControlActive();
    if (m_controlActive == active) return;
    m_controlActive = active;
    emit controlActiveChanged(active);
}
void Page3ViewModel::executeCapture(const std::function<void()>& execute)
{
    if (!m_controlAllowed) return;
    QPointer<Page3ViewModel> alive(this);
    ++m_capturePreparing;
    const auto finish = qScopeGuard([alive] {
        if (!alive) return;
        --alive->m_capturePreparing;
        // The capture lease ends on a worker thread. Observe it on the UI thread
        // only while capture is active; do not call a destroyed QObject from a worker.
        auto* timer = new QTimer(alive);
        timer->setInterval(50);
        QObject::connect(timer, &QTimer::timeout, alive, [alive, timer] {
            if (!alive->m_captureSession->isBusy()) {
                timer->stop();
                timer->deleteLater();
                alive->updateControlActivity();
            }
        });
        if (alive->m_captureSession->isBusy()) timer->start();
        else timer->deleteLater();
        alive->updateControlActivity();
    });
    updateControlActivity();
    if (alive) execute();
}

void Page3ViewModel::publishXmlLoaded()
{
    restoreFromModel();
    updateUIAfterLoad();
}
