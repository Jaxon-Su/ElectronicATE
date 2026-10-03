#include "page3viewmodel.h"
#include <QDebug>
#include "messageservice.h"
#include <QPointer>
#include <QTimer>
#include <QScopeGuard>
#include "instrumentconfigvalidator.h"
#include <QDir>
#include "pngcapturecommand.h"
#include "csvcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "allwfmcapturecommand.h"
#include "savedirpreference.h"
#include "oscilloscopemanager.h"
#include "instrumentoperationrunner.h"

Page3ViewModel::Page3ViewModel(Page3Model *p3, Page3Operations operations, QObject *parent)
    : QObject(parent), m_model(p3), m_scopes(operations.connectScopes), m_operations(std::move(operations))
{
    connect(&m_triggerBinding, &TriggerBinding::reconnectRequested, this,
            &Page3ViewModel::reconnectOscilloscopes, Qt::QueuedConnection);
    connect(&m_triggerBinding, &TriggerBinding::commandActivityChanged, this,
            &Page3ViewModel::updateControlActivity);
    connect(&m_scopes, &ScopeSessionCoordinator::stateChanged, this, &Page3ViewModel::updateControlActivity);
    connect(&m_scopes, &ScopeSessionCoordinator::configurationAccepted, this,
            [this](const Page1Config &config)
            {
                m_page1Config = config;
                emit page1ConfigChanged(config);
            });
    connect(&m_scopes, &ScopeSessionCoordinator::scopesAboutToReset, this,
            [this]
            {
                m_triggerBinding.bindInstrument(nullptr);
                m_triggerBinding.setPollingLeaseFactory({});
            });
    connect(&m_scopes, &ScopeSessionCoordinator::scopesChanged, this,
            [this]
            {
                if (m_triggerBinding.hasController() && m_scopes.current())
                    connectTriggerController();
            });
    connect(&m_scopes, &ScopeSessionCoordinator::connectionFailed, this, [this](const QString &error)
            { MessageService::instance().showWarning(tr("Connection Failed"), error); });
}

Page3ViewModel::~Page3ViewModel()
{
    m_operationQueue.close();
    cleanupTriggerResources();
}
void Page3ViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0)
        return;
    emit headersChanged(TableHeaderBuilder::buildIndexHeaders(maxOutput));
}

void Page3ViewModel::setNameList(const QStringList &names) { emit rowLabelsChanged(names); }

void Page3ViewModel::updateTitles(TableKind type, const QStringList &titles)
{
    switch (type)
    {
    case TableKind::Input:
        if (m_model)
            m_model->setInputTitles(titles);
        m_inputTitles = titles;
        emit titlesUpdated(TableKind::Input, titles);
        break;
    case TableKind::Load:
        if (m_model)
            m_model->setLoadTitles(titles);
        m_loadTitles = titles;
        emit titlesUpdated(TableKind::Load, titles);
        break;
    case TableKind::DyLoad:
        if (m_model)
            m_model->setDyLoadTitles(titles);
        m_dyloadTitles = titles;
        emit titlesUpdated(TableKind::DyLoad, titles);
        break;
    case TableKind::Relay:
        if (m_model)
            m_model->setRelayTitles(titles);
        m_relayTitles = titles;
        emit titlesUpdated(TableKind::Relay, titles);
        break;
    case TableKind::Dc:
        break;
    }
}

std::shared_ptr<Oscilloscope> Page3ViewModel::borrowOscilloscope()
{
    if (!m_controlAllowed || hasActiveControl() || m_triggerBinding.isOperationActive())
        return {};
    auto scope = m_scopes.borrowForTest();
    if (scope)
        m_triggerBinding.setSuspended(true);
    return scope;
}

void Page3ViewModel::onPage1ConfigChanged(const Page1Config &config)
{
    QPointer<Page3ViewModel> alive(this);
    m_scopes.queueConfiguration(config);
    if (alive)
        updateControlActivity();
}

void Page3ViewModel::onInputDataChanged(const QVector<InputRow> &rows) { m_conditions.inputRows = rows; }

void Page3ViewModel::onConditionsChanged(const TestConditionSnapshot &snapshot)
{
    m_conditions = snapshot;
    refreshDcInputs();
    onInputDataChanged(snapshot.inputRows);
    onLoadMetaChanged(snapshot.loadMeta);
    onLoadRowsChanged(snapshot.loadRows);
    onDynamicMetaChanged(snapshot.dynamicMeta);
    onDynamicRowsChanged(snapshot.dynamicRows);
    // This handler publishes titles; call only after all condition data is set.
    onRelayRowsChanged(snapshot.relayRows);
}

void Page3ViewModel::onLoadMetaChanged(const LoadMetaRow &meta)
{
    if (m_model)
        m_model->setPage2LoadMetaDataChanged(meta);
    m_conditions.loadMeta = meta;
}

void Page3ViewModel::onLoadRowsChanged(const QVector<LoadDataRow> &rows)
{
    if (m_model)
        m_model->setPage2LoadRowsChanged(rows);
    m_conditions.loadRows = rows;
}

void Page3ViewModel::onRelayRowsChanged(const QVector<RelayDataRow> &rows)
{
    if (m_model)
        m_model->setPage2RelayRowsChanged(rows);
    m_conditions.relayRows = rows;

    QStringList titles;
    for (const auto &row : std::as_const(rows))
    {
        titles << row.label;
    }
    m_relayTitles = titles;
    emit titlesUpdated(TableKind::Relay, titles);
}

void Page3ViewModel::onDynamicMetaChanged(const DynamicMetaRow &meta)
{
    if (m_model)
        m_model->setPage2DynamicMetaChanged(meta);
    m_conditions.dynamicMeta = meta;
}

void Page3ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow> &rows)
{
    if (m_model)
        m_model->setPage2DynamicRowsChanged(rows);
    m_conditions.dynamicRows = rows;
}

void Page3ViewModel::onInputToggled(bool on)
{
    if (on)
    {
        handleInput(InputAction::PowerOn);
    }
    else
    {
        handleInput(InputAction::PowerOff);
    }
}

void Page3ViewModel::onInputChanged() { handleInput(InputAction::Change); }

void Page3ViewModel::onLoadToggled(bool on)
{
    if (on)
    {
        handleLoad(LoadAction::LoadOn);
    }
    else
    {
        handleLoad(LoadAction::LoadOff);
    }
}

void Page3ViewModel::onLoadChanged() { handleLoad(LoadAction::Change); }

void Page3ViewModel::onDyloadToggled(bool on)
{
    if (on)
    {
        handleDyLoad(DynamicLoadAction::LoadOn);
    }
    else
    {
        handleDyLoad(DynamicLoadAction::LoadOff);
    }
}

void Page3ViewModel::onDyLoadChanged() { handleDyLoad(DynamicLoadAction::Change); }

void Page3ViewModel::onRelayToggled(bool on)
{
    if (on)
    {
        handleRelay(RelayAction::RelayOn);
    }
    else
    {
        handleRelay(RelayAction::RelayOff);
    }
}

void Page3ViewModel::onRelayChanged() { handleRelay(RelayAction::Change); }

bool Page3ViewModel::tryBeginLoadOperation(const char *context)
{
    if (m_loadOperationBusy)
    {
        qWarning() << "[Page3ViewModel]" << context << "ignored because a load operation is still running";
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

void Page3ViewModel::onSelected(TableKind type, int idx, const QString &txt)
{
    m_selections[type] = {idx, txt};
}

void Page3ViewModel::onTriggerWidgetCreated(const QString &modelName, QObject *triggerController)
{
    auto *controller = qobject_cast<ITriggerController *>(triggerController);
    if (m_triggerBinding.attach(controller, modelName))
    {
        connectTriggerController();
    }
    else if (controller)
    {
        qWarning() << "[Page3ViewModel] Controller model mismatch:" << controller->getSupportedModel() << "vs"
                   << modelName;
    }
}

void Page3ViewModel::onTriggerWidgetDestroyed() { cleanupTriggerResources(); }

void Page3ViewModel::connectTriggerController()
{
    if (!m_triggerBinding.hasController())
    {
        qWarning() << "[Page3ViewModel] No trigger controller available";
        return;
    }
    const QString modelName = m_triggerBinding.modelName();
    auto osc = m_scopes.select(modelName);
    if (!osc)
    {
        qWarning() << "[Page3ViewModel] Oscilloscope not found for model:" << modelName;
        m_triggerBinding.bindInstrument(nullptr);
        return;
    }
    m_triggerBinding.setSuspended(!m_controlAllowed || m_scopes.testBusy() || m_capturePreparing ||
                                  m_scopes.captureBusy());
    m_triggerBinding.bindInstrument(osc.get());
    m_triggerBinding.setPollingLeaseFactory(m_scopes.pollingLeaseFactory());
}

void Page3ViewModel::reconnectOscilloscopes()
{
    if (m_scopes.hasPendingConfiguration() || m_scopes.isConfigurationBusy())
        return;
    bool hasScope = false;
    for (const auto &instrument : m_page1Config.instruments)
        hasScope |= instrument.type == "Oscilloscope" && instrument.enabled &&
                    !instrument.modelName.trimmed().isEmpty();
    if (!hasScope)
        return;
    qDebug() << "[Page3VM] Oscilloscope reconnect requested";
    // 複用現有 Page1 設定重建示波器連線（不需要使用者改動 Page1）
    onPage1ConfigChanged(m_page1Config);
}

void Page3ViewModel::cleanupTriggerResources() { m_triggerBinding.detach(); }

void Page3ViewModel::writeXml(QXmlStreamWriter &writer) const
{
    const auto &sel = m_selections;
    m_model->setSelectedInputState(sel.value(TableKind::Input).index, sel.value(TableKind::Input).text);
    m_model->setSelectedLoadState(sel.value(TableKind::Load).index, sel.value(TableKind::Load).text);
    m_model->setSelectedDyLoadState(sel.value(TableKind::DyLoad).index, sel.value(TableKind::DyLoad).text);
    m_model->setSelectedRelayState(sel.value(TableKind::Relay).index, sel.value(TableKind::Relay).text);
    m_model->writeXml(writer);
}

void Page3ViewModel::loadXml(QXmlStreamReader &reader)
{
    m_model->loadXml(reader);
    if (reader.hasError())
        return;
    restoreFromModel();
    if (!signalsBlocked())
        updateUIAfterLoad();
}

void Page3ViewModel::restoreFromModel()
{
    if (!m_model)
        return;

    m_inputTitles = m_model->getInputTitles();
    m_loadTitles = m_model->getLoadTitles();
    m_dyloadTitles = m_model->getDyLoadTitles();
    m_relayTitles = m_model->getRelayTitles();

    m_conditions.loadMeta = m_model->getLoadMetaData();
    m_conditions.loadRows = m_model->getLoadRowsData();
    {
        auto newMeta = m_model->getDynamicMetaData();
        // Page3 未提供 T1T2 時，保留 Page2 的條件值。
        if (newMeta.t1t2.isEmpty() && !m_conditions.dynamicMeta.t1t2.isEmpty())
            newMeta.t1t2 = m_conditions.dynamicMeta.t1t2;
        m_conditions.dynamicMeta = newMeta;
    }
    m_conditions.dynamicRows = m_model->getDynamicRowsData();
    m_conditions.relayRows = m_model->getRelayRowsData();

    m_selections[TableKind::Input] = {m_model->getSelectedInputIndex(), m_model->getSelectedInputText()};
    m_selections[TableKind::Load] = {m_model->getSelectedLoadIndex(), m_model->getSelectedLoadText()};
    m_selections[TableKind::DyLoad] = {m_model->getSelectedDyLoadIndex(), m_model->getSelectedDyLoadText()};
    m_selections[TableKind::Relay] = {m_model->getSelectedRelayIndex(), m_model->getSelectedRelayText()};
}

void Page3ViewModel::updateUIAfterLoad()
{
    emit titlesUpdated(TableKind::Input, m_inputTitles);
    emit titlesUpdated(TableKind::Load, m_loadTitles);
    emit titlesUpdated(TableKind::DyLoad, m_dyloadTitles);
    emit titlesUpdated(TableKind::Relay, m_relayTitles);

    QTimer::singleShot(100, this, [this]() { restoreUISelections(); });
}

void Page3ViewModel::restoreUISelections()
{
    refreshDcInputs();
    for (auto it = m_selections.constBegin(); it != m_selections.constEnd(); ++it)
    {
        if (it.value().index >= 0)
            emit restoreSelections(it.key(), it.value().index, it.value().text);
    }
}

void Page3ViewModel::onDcSelected(int source, int index, const QString &text)
{
    if (source >= 0 && source < 3)
        m_model->setDcSelection(source, index, text);
}

void Page3ViewModel::onDcInputToggled(int source, bool on)
{
    handleDcInput(source, on ? InputAction::PowerOn : InputAction::PowerOff);
}

void Page3ViewModel::onDcInputChanged(int source) { handleDcInput(source, InputAction::Change); }

void Page3ViewModel::handleDcInput(int source, InputAction action)
{
    handleDcOperation(source, action, false);
}

void Page3ViewModel::onDcGroupToggled(bool on)
{
    handleDcOperation(0, on ? InputAction::PowerOn : InputAction::PowerOff, true);
}

void Page3ViewModel::onDcGroupChanged() { handleDcOperation(0, InputAction::Change, true); }

void Page3ViewModel::handleDcOperation(int source, InputAction action, bool grouped)
{
    if (source < 0 || source >= 3)
        return;
    QVector<int> targets;
    if (grouped)
    {
        for (int i = 0; i < qBound(1, m_page1Config.dcInputs, 3); ++i)
            for (const auto &instrument : m_page1Config.instruments)
                if (instrument.type == "InputDCSource" && instrument.enabled &&
                    instrument.name == QString("DC Source%1").arg(i + 1))
                {
                    targets.append(i);
                    break;
                }
    }
    else
    {
        targets.append(source);
    }
    bool busy = false;
    for (int i : targets)
        busy |= m_outputs.busy(TableKind::Dc, i);
    if (!m_controlAllowed || busy)
    {
        emit dcOutputStateChanged(source, outputMayBeOn(m_outputs.state(TableKind::Dc, source)));
        return;
    }
    DcRow row;
    const auto &rows = m_conditions.dcSourceRows(source);
    const int rowCount = grouped ? qMax(m_conditions.dcRows.size(),
                                        qMax(m_conditions.dcRows2.size(), m_conditions.dcRows3.size()))
                                 : rows.size();
    const int index = m_model->dcSelection(source).index;
    if (action != InputAction::PowerOff && (index < 0 || index >= rowCount))
    {
        QPointer<Page3ViewModel> alive(this);
        emit dcOutputStateChanged(source, outputMayBeOn(m_outputs.state(TableKind::Dc, source)));
        if (alive)
            MessageService::instance().showWarning(
                tr("DC Input Selection Error"),
                tr("Select a condition for DC Source%1 first.").arg(source + 1));
        return;
    }
    DcGroup group;
    if (action != InputAction::PowerOff)
    {
        row = rows.value(index);
        for (int i = 0; i < 3; ++i)
            group[i] = m_conditions.dcSourceRows(i).value(index);
    }
    if ((grouped && !m_operations.dcGroup) || (!grouped && !m_operations.dcInput))
    {
        emit dcOutputStateChanged(source, outputMayBeOn(m_outputs.state(TableKind::Dc, source)));
        return;
    }
    const auto cfg = m_page1Config;
    const auto run = m_operations.dcInput;
    const auto runGroup = m_operations.dcGroup;
    QVector<OutputOperationState::Channel> channels;
    for (int i : targets) channels.append({TableKind::Dc, i});
    const auto ticket = m_outputs.begin(channels, action == InputAction::Change ? std::nullopt
                                                        : std::optional<bool>(action == InputAction::PowerOn));
    QPointer<Page3ViewModel> alive(this);
    for (int i : targets)
    {
        emit dcOperationBusyChanged(i, true);
        if (!alive)
            return;
    }
    updateControlActivity();
    if (!alive)
        return;
    m_operationQueue.submit(
        [cfg, source, row, group, action, run, runGroup, grouped]
        { return grouped ? runGroup(cfg, group, action) : run(cfg, source, row, action); },
        [this, source, ticket, targets, grouped](const InstrumentOperationResult &result)
        {
            QPointer<Page3ViewModel> alive(this);
            m_outputs.complete(ticket, result);
            for (int i : targets)
            {
                emit dcOutputStateChanged(i, outputMayBeOn(m_outputs.state(TableKind::Dc, i)));
                if (!alive)
                    return;
                emit dcOperationBusyChanged(i, false);
                if (!alive)
                    return;
            }
            updateControlActivity();
            if (alive && !result.success)
                MessageService::instance().showWarning(
                    grouped ? tr("DC Group Control Error") : tr("DC Source%1 Control Error").arg(source + 1),
                    result.errorMessage);
        });
}

void Page3ViewModel::refreshDcInputs()
{
    if (m_operations.dcGroup)
    {
        QStringList titles;
        const int count =
            qMax(m_conditions.dcRows.size(), qMax(m_conditions.dcRows2.size(), m_conditions.dcRows3.size()));
        for (int group = 0; group < count; ++group)
        {
            titles << m_conditions.dcNames.value(group);
        }
        int index = m_model->dcSelection(0).index;
        if (index < 0 || index >= titles.size() || titles[index].isEmpty())
        {
            index = -1;
            for (int i = 0; i < titles.size(); ++i)
                if (!titles[i].isEmpty())
                {
                    index = i;
                    break;
                }
        }
        m_model->setDcSelection(0, index, index < 0 ? QString() : titles[index]);
        emit dcInputUpdated(0, titles, index);
        return;
    }
    for (int source = 0; source < 3; ++source)
    {
        QStringList titles;
        const auto &rows = m_conditions.dcSourceRows(source);
        for (const auto &row : rows)
            titles.append(dcInputTitle(row));
        const auto selection = m_model->dcSelection(source);
        int index = selection.index >= 0 && selection.index < titles.size() ? selection.index : -1;
        if (index < 0 || titles[index].isEmpty())
        {
            for (int i = 0; i < titles.size(); ++i)
            {
                if (!titles[i].isEmpty())
                {
                    index = i;
                    break;
                }
            }
        }
        m_model->setDcSelection(source, index, index < 0 ? QString() : titles[index]);
        emit dcInputUpdated(source, titles, index);
    }
}

void Page3ViewModel::rejectOperation(const QString &title, const QString &message, TableKind type)
{
    QPointer<Page3ViewModel> alive(this);
    emit forceOff(type);
    if (alive)
        emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
    if (alive)
        MessageService::instance().showWarning(title, message);
}

void Page3ViewModel::handleInput(InputAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &inputSel = m_selections.value(TableKind::Input);
    auto validResult = InstrumentConfigValidator::validateInput(m_page1Config, inputSel.text);
    if (!validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Input);
        return;
    }

    const Page1Config cfg = m_page1Config;
    if (inputSel.index < 0 || inputSel.index >= m_conditions.inputRows.size())
    {
        rejectOperation("Input Selection Error", "Selected input row is out of range.", TableKind::Input);
        return;
    }

    const InputRow inputRow = m_conditions.inputRows.at(inputSel.index);
    startInstrumentOperation(
        [cfg, inputRow, action, run = m_operations.input] { return run(cfg, inputRow, action); },
        "Input Configuration Error", TableKind::Input, action == InputAction::PowerOn, false,
        action == InputAction::Change ? std::nullopt : std::optional<bool>(action == InputAction::PowerOn));
}

void Page3ViewModel::handleRelay(RelayAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &relaySel = m_selections.value(TableKind::Relay);
    auto validResult = InstrumentConfigValidator::validateRelay(m_page1Config, relaySel.text);
    if (!validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Relay);
        return;
    }

    const Page1Config cfg = m_page1Config;
    const auto relayRows = m_conditions.relayRows;
    const int condIdx = relaySel.index;
    startInstrumentOperation(
        [cfg, relayRows, condIdx, action, run = m_operations.relay]
        { return run(cfg, relayRows, condIdx, action); }, "Relay Communication Error", TableKind::Relay,
        action == RelayAction::RelayOn, false,
        action == RelayAction::Change ? std::nullopt : std::optional<bool>(action == RelayAction::RelayOn));
}

void Page3ViewModel::handleLoad(LoadAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &loadSel = m_selections.value(TableKind::Load);
    auto validResult = InstrumentConfigValidator::validateLoad(m_page1Config, loadSel.text);
    if (!validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Load);
        return;
    }
    if (!tryBeginLoadOperation("handleLoad"))
        return;

    const Page1Config cfg = m_page1Config;
    const auto loadRows = m_conditions.loadRows;
    const int condIdx = loadSel.index;
    const auto meta = m_conditions.loadMeta;
    const bool syncEnabled = m_syncEnabled;
    startInstrumentOperation(
        [cfg, loadRows, condIdx, meta, action, syncEnabled, run = m_operations.load]
        { return run(cfg, loadRows, condIdx, meta, action, syncEnabled); }, "Load Configuration Error",
        TableKind::Load, action == LoadAction::LoadOn, true,
        action == LoadAction::Change ? std::nullopt : std::optional<bool>(action == LoadAction::LoadOn));
}

void Page3ViewModel::handleDyLoad(DynamicLoadAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &dyLoadSel = m_selections.value(TableKind::DyLoad);
    auto validResult = InstrumentConfigValidator::validateDyLoad(m_page1Config, dyLoadSel.text);
    if (!validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::DyLoad);
        return;
    }
    if (!tryBeginLoadOperation("handleDyLoad"))
        return;

    const bool syncEnabled = m_syncEnabled;
    const bool syncDirty = m_syncDirty;
    if (syncEnabled || syncDirty)
        m_syncDirty = false;

    const Page1Config cfg = m_page1Config;
    const auto dyRows = m_conditions.dynamicRows;
    const int condIdx = dyLoadSel.index;
    const auto meta = m_conditions.dynamicMeta;
    startInstrumentOperation(
        [cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty, run = m_operations.dynamic]
        { return run(cfg, dyRows, condIdx, meta, action, syncEnabled, syncDirty); },
        "Dynamic Load Configuration Error", TableKind::DyLoad, action == DynamicLoadAction::LoadOn, true,
        action == DynamicLoadAction::Change ? std::nullopt
                                            : std::optional<bool>(action == DynamicLoadAction::LoadOn));
}

void Page3ViewModel::OnWaveformCaptured()
{
    PngCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnCsvCaptured(int channel)
{
    CsvCaptureCommand cmd(buildCaptureContext(channel));
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnAllCsvCaptured()
{
    AllCsvCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnAllWfmCaptured()
{
    AllWfmCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void Page3ViewModel::OnWfmCaptured(int channel)
{
    WfmCaptureCommand cmd(buildCaptureContext(channel));
    executeCapture([&cmd] { cmd.execute(); });
}

CaptureContext Page3ViewModel::buildCaptureContext(int channel) const
{
    CaptureContext ctx;
    ctx.oscilloscope = m_scopes.current();
    ctx.captureSession = m_scopes.captureSession();
    ctx.lastSaveDir = SaveDirPreference::load();
    ctx.selectSaveFile = m_captureFileSelector;
    ctx.onSaveDirChanged = [](const QString &filePath) { SaveDirPreference::save(filePath); };
    ctx.captureChannel = channel;
    return ctx;
}

void Page3ViewModel::onSyncChanged(bool enabled)
{
    m_syncEnabled = enabled;

    if (!enabled)
    {
        m_syncDirty = true;
    }

    qDebug() << "[Page3VM] SyncDynamic enabled=" << enabled << "dirty=" << m_syncDirty;
}

void Page3ViewModel::validateXml(QXmlStreamReader &reader) const
{
    Page3Model candidate;
    candidate.loadXml(reader);
}

void Page3ViewModel::startInstrumentOperation(std::function<InstrumentOperationResult()> work,
                                              const QString &errorTitle, TableKind type,
                                              bool forceOffOnFailure, bool releaseLoadBusy,
                                              std::optional<bool> outputOn)
{
    QPointer<Page3ViewModel> guard(this);
    const auto ticket = m_outputs.begin({{type, 0}}, outputOn);
    updateControlActivity();
    if (!guard)
        return;
    emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
    if (!guard)
        return;
    m_operationQueue.submit(
        std::move(work),
        [this, errorTitle, type, forceOffOnFailure, releaseLoadBusy,
         ticket](const InstrumentOperationResult &result)
        {
            QPointer<Page3ViewModel> alive(this);
            m_outputs.complete(ticket, result);
            if (!result.success)
            {
                if (forceOffOnFailure)
                    emit forceOff(type);
            }
            if (!alive)
                return;
            emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
            if (alive && releaseLoadBusy)
                finishLoadOperation();
            if (!alive)
                return;
            updateControlActivity();
            // Settle the UI and ownership before a modal warning starts a nested event loop.
            if (alive && !result.success && !result.errorMessage.isEmpty())
                MessageService::instance().showWarning(errorTitle, result.errorMessage);
        });
}

bool Page3ViewModel::isControlActive() const { return hasActiveControl() || isConfigurationBusy(); }

bool Page3ViewModel::scopeControlsEnabled() const
{
    const auto scope = m_scopes.current();
    return m_controlAllowed && !isConfigurationBusy() && !m_scopes.hasPendingConfiguration() &&
           !m_scopes.testBusy() && !m_capturePreparing && !m_scopes.captureBusy() &&
           !m_triggerBinding.hasActiveCommand() && scope && scope->isConnected();
}

bool Page3ViewModel::hasActiveControl() const
{
    if (m_triggerBinding.hasActiveCommand())
        return true;
    return m_outputs.hasActiveControl() || m_capturePreparing || m_scopes.captureBusy();
}

bool Page3ViewModel::hasPendingControlOperation() const
{
    return m_outputs.hasPendingOperations() || m_triggerBinding.hasActiveCommand() ||
           m_capturePreparing || m_scopes.captureBusy();
}
void Page3ViewModel::setControlAllowed(bool allowed)
{
    if (m_controlAllowed == allowed)
        return;
    m_controlAllowed = allowed;
    updateControlActivity();
}
void Page3ViewModel::updateControlActivity()
{
    QPointer<Page3ViewModel> alive(this);
    m_scopes.setBlocked(!m_controlAllowed || hasActiveControl());
    m_triggerBinding.setSuspended(!m_controlAllowed || isConfigurationBusy() || m_scopes.testBusy() ||
                                  m_capturePreparing || m_scopes.captureBusy());
    const bool scopeEnabled = scopeControlsEnabled();
    if (m_scopeControlsEnabled != scopeEnabled)
    {
        m_scopeControlsEnabled = scopeEnabled;
        emit scopeControlsEnabledChanged(scopeEnabled);
        if (!alive)
            return;
    }
    const bool active = hasActiveControl();
    const bool connecting = isConfigurationBusy();
    if (m_hardwareControlActive == active && m_configurationBusy == connecting)
        return;
    // Ownership can change while a scope connection keeps the aggregate busy.
    m_hardwareControlActive = active;
    m_configurationBusy = connecting;
    emit controlActiveChanged(active || connecting);
}
void Page3ViewModel::executeCapture(const std::function<void()> &execute)
{
    if (m_scopes.pollingBusy())
        return;
    if (!scopeControlsEnabled())
        return;
    QPointer<Page3ViewModel> alive(this);
    ++m_capturePreparing;
    const auto finish = qScopeGuard(
        [alive]
        {
            if (!alive)
                return;
            --alive->m_capturePreparing;
            alive->m_scopes.observeLeases();
            alive->updateControlActivity();
        });
    updateControlActivity();
    if (alive)
        execute();
}

void Page3ViewModel::publishXmlLoaded()
{
    restoreFromModel();
    refreshDcInputs();
    updateUIAfterLoad();
}
