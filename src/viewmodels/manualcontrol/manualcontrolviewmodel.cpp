#include "manualcontrolviewmodel.h"
#include <QDebug>
#include "messageservice.h"
#include <QPointer>
#include <QTimer>
#include <QScopeGuard>
#include "instrumentconfigvalidator.h"
#include <QDir>
#include "pngcapturecommand.h"
#include "waveformcaptureworkflow.h"
#include "allcsvcapturecommand.h"
#include "allwfmcapturecommand.h"
#include "savedirpreference.h"
#include "oscilloscopemanager.h"
#include "instrumentoperationrunner.h"
#include "shortcircuitexit.h"
#include "configuredinputs.h"

ManualControlViewModel::ManualControlViewModel(ManualControlModel *manualControl, ManualControlOperations operations, QObject *parent)
    : QObject(parent), m_model(manualControl), m_scopes(operations.connectScopes), m_operations(std::move(operations))
{
    connect(&m_triggerBinding, &TriggerBinding::reconnectRequested, this,
            &ManualControlViewModel::reconnectOscilloscopes, Qt::QueuedConnection);
    connect(&m_triggerBinding, &TriggerBinding::commandActivityChanged, this,
            &ManualControlViewModel::updateControlActivity);
    connect(&m_scopes, &ScopeSessionCoordinator::stateChanged, this, &ManualControlViewModel::updateControlActivity);
    connect(&m_scopes, &ScopeSessionCoordinator::configurationAccepted, this,
            [this](const InstrumentSetupConfig &config)
            {
                m_instrumentSetupConfig = config;
                emit instrumentSetupConfigChanged(config);
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

ManualControlViewModel::~ManualControlViewModel()
{
    m_operationQueue.close();
    cleanupTriggerResources();
}
void ManualControlViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0)
        return;
    emit headersChanged(TableHeaderBuilder::buildIndexHeaders(maxOutput));
}

void ManualControlViewModel::setNameList(const QStringList &names) { emit rowLabelsChanged(names); }

void ManualControlViewModel::updateTitles(TableKind type, const QStringList &titles)
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

std::shared_ptr<Oscilloscope> ManualControlViewModel::borrowOscilloscope()
{
    if (!m_controlAllowed || hasActiveControl() || m_triggerBinding.isOperationActive())
        return {};
    auto scope = m_scopes.borrowForTest();
    if (scope)
        m_triggerBinding.setSuspended(true);
    return scope;
}

void ManualControlViewModel::onInstrumentSetupConfigChanged(const InstrumentSetupConfig &config)
{
    QPointer<ManualControlViewModel> alive(this);
    m_scopes.queueConfiguration(config);
    if (alive)
        updateControlActivity();
}

void ManualControlViewModel::adoptUncertainOutputs(const InstrumentSetupConfig &config, const QVector<TableKind> &outputs,
                                         const ShortCircuitProtection &protection)
{
    QPointer<ManualControlViewModel> alive(this);
    // Recover using the exact Run instrument snapshot, even if a newer InstrumentSetup
    // configuration is queued. UNKNOWN keeps configuration/Run/close interlocks active.
    m_instrumentSetupConfig = config;
    if (protection.active()) m_shortCircuit = protection;
    for (auto kind : outputs) {
        if (kind == TableKind::Dc) {
            for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source)
                for (const auto &instrument : config.instruments)
                    if (instrument.enabled && instrument.type == "InputDCSource" &&
                        instrument.name == QString("DC Source%1").arg(source + 1)) {
                        m_outputs.markUnknown({kind, source});
                        emit dcOutputStateChanged(source, true);
                        if (!alive) return;
                    }
        } else {
            m_outputs.markUnknown({kind, 0});
            emit restoreOutputState(kind, true);
            if (!alive) return;
        }
    }
    updateControlActivity();
}

void ManualControlViewModel::onInputDataChanged(const QVector<InputRow> &rows) { m_conditions.inputRows = rows; }

void ManualControlViewModel::onConditionsChanged(const TestConditionSnapshot &snapshot)
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

void ManualControlViewModel::onLoadMetaChanged(const LoadMetaRow &meta)
{
    if (m_model)
        m_model->setTestConditionsLoadMetaDataChanged(meta);
    m_conditions.loadMeta = meta;
}

void ManualControlViewModel::onLoadRowsChanged(const QVector<LoadDataRow> &rows)
{
    if (m_model)
        m_model->setTestConditionsLoadRowsChanged(rows);
    m_conditions.loadRows = rows;
}

void ManualControlViewModel::onRelayRowsChanged(const QVector<RelayDataRow> &rows)
{
    if (m_model)
        m_model->setTestConditionsRelayRowsChanged(rows);
    m_conditions.relayRows = rows;

    QStringList titles;
    for (const auto &row : std::as_const(rows))
    {
        titles << row.label;
    }
    m_relayTitles = titles;
    emit titlesUpdated(TableKind::Relay, titles);
}

void ManualControlViewModel::onDynamicMetaChanged(const DynamicMetaRow &meta)
{
    if (m_model)
        m_model->setTestConditionsDynamicMetaChanged(meta);
    m_conditions.dynamicMeta = meta;
}

void ManualControlViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow> &rows)
{
    if (m_model)
        m_model->setTestConditionsDynamicRowsChanged(rows);
    m_conditions.dynamicRows = rows;
}

void ManualControlViewModel::onInputToggled(bool on)
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

void ManualControlViewModel::onInputChanged() { handleInput(InputAction::Change); }

void ManualControlViewModel::onLoadToggled(bool on)
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

void ManualControlViewModel::onLoadChanged() { handleLoad(LoadAction::Change); }

void ManualControlViewModel::onDyloadToggled(bool on)
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

void ManualControlViewModel::onDyLoadChanged() { handleDyLoad(DynamicLoadAction::Change); }

void ManualControlViewModel::onRelayToggled(bool on)
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

void ManualControlViewModel::onRelayChanged() { handleRelay(RelayAction::Change); }

bool ManualControlViewModel::tryBeginLoadOperation(const char *context)
{
    if (m_loadOperationBusy)
    {
        qWarning() << "[ManualControlViewModel]" << context << "ignored because a load operation is still running";
        emit loadOperationBusyChanged(true);
        return false;
    }

    m_loadOperationBusy = true;
    QPointer<ManualControlViewModel> alive(this);
    emit loadOperationBusyChanged(true);
    return !alive.isNull();
}

void ManualControlViewModel::finishLoadOperation()
{
    m_loadOperationBusy = false;
    emit loadOperationBusyChanged(false);
}

void ManualControlViewModel::onSelected(TableKind type, int idx, const QString &txt)
{
    m_selections[type] = {idx, txt};
}

void ManualControlViewModel::onTriggerWidgetCreated(const QString &modelName, QObject *triggerController)
{
    auto *controller = qobject_cast<ITriggerController *>(triggerController);
    if (m_triggerBinding.attach(controller, modelName))
    {
        connectTriggerController();
    }
    else if (controller)
    {
        qWarning() << "[ManualControlViewModel] Controller model mismatch:" << controller->getSupportedModel() << "vs"
                   << modelName;
    }
}

void ManualControlViewModel::onTriggerWidgetDestroyed() { cleanupTriggerResources(); }

void ManualControlViewModel::connectTriggerController()
{
    if (!m_triggerBinding.hasController())
    {
        qWarning() << "[ManualControlViewModel] No trigger controller available";
        return;
    }
    const QString modelName = m_triggerBinding.modelName();
    auto osc = m_scopes.select(modelName);
    if (!osc)
    {
        qWarning() << "[ManualControlViewModel] Oscilloscope not found for model:" << modelName;
        m_triggerBinding.bindInstrument(nullptr);
        return;
    }
    m_triggerBinding.setSuspended(!m_controlAllowed || m_scopes.testBusy() || m_capturePreparing ||
                                  m_scopes.captureBusy());
    m_triggerBinding.bindInstrument(osc.get());
    m_triggerBinding.setPollingLeaseFactory(m_scopes.pollingLeaseFactory());
}

void ManualControlViewModel::reconnectOscilloscopes()
{
    if (m_scopes.hasPendingConfiguration() || m_scopes.isConfigurationBusy())
        return;
    bool hasScope = false;
    for (const auto &instrument : m_instrumentSetupConfig.instruments)
        hasScope |= instrument.type == "Oscilloscope" && instrument.enabled &&
                    !instrument.modelName.trimmed().isEmpty();
    if (!hasScope)
        return;
    qDebug() << "[ManualControlVM] Oscilloscope reconnect requested";
    // 複用現有 InstrumentSetup 設定重建示波器連線（不需要使用者改動 InstrumentSetup）
    onInstrumentSetupConfigChanged(m_instrumentSetupConfig);
}

void ManualControlViewModel::cleanupTriggerResources() { m_triggerBinding.detach(); }

void ManualControlViewModel::writeXml(QXmlStreamWriter &writer) const
{
    const auto &sel = m_selections;
    m_model->setSelectedInputState(sel.value(TableKind::Input).index, sel.value(TableKind::Input).text);
    m_model->setSelectedLoadState(sel.value(TableKind::Load).index, sel.value(TableKind::Load).text);
    m_model->setSelectedDyLoadState(sel.value(TableKind::DyLoad).index, sel.value(TableKind::DyLoad).text);
    m_model->setSelectedRelayState(sel.value(TableKind::Relay).index, sel.value(TableKind::Relay).text);
    m_model->writeXml(writer);
}

void ManualControlViewModel::loadXml(QXmlStreamReader &reader)
{
    m_model->loadXml(reader);
    if (reader.hasError())
        return;
    restoreFromModel();
    if (!signalsBlocked())
        updateUIAfterLoad();
}

void ManualControlViewModel::restoreFromModel()
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
        // ManualControl 未提供 T1T2 時，保留 TestConditions 的條件值。
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

void ManualControlViewModel::updateUIAfterLoad()
{
    emit titlesUpdated(TableKind::Input, m_inputTitles);
    emit titlesUpdated(TableKind::Load, m_loadTitles);
    emit titlesUpdated(TableKind::DyLoad, m_dyloadTitles);
    emit titlesUpdated(TableKind::Relay, m_relayTitles);

    QTimer::singleShot(100, this, [this]() { restoreUISelections(); });
}

void ManualControlViewModel::restoreUISelections()
{
    refreshDcInputs();
    for (auto it = m_selections.constBegin(); it != m_selections.constEnd(); ++it)
    {
        if (it.value().index >= 0)
            emit restoreSelections(it.key(), it.value().index, it.value().text);
    }
}

void ManualControlViewModel::onDcSelected(int source, int index, const QString &text)
{
    if (source >= 0 && source < 3)
        m_model->setDcSelection(source, index, text);
}

void ManualControlViewModel::onDcInputToggled(int source, bool on)
{
    handleDcInput(source, on ? InputAction::PowerOn : InputAction::PowerOff);
}

void ManualControlViewModel::onDcInputChanged(int source) { handleDcInput(source, InputAction::Change); }

void ManualControlViewModel::handleDcInput(int source, InputAction action)
{
    handleDcOperation(source, action, false);
}

void ManualControlViewModel::onDcGroupToggled(bool on)
{
    handleDcOperation(0, on ? InputAction::PowerOn : InputAction::PowerOff, true);
}

void ManualControlViewModel::onDcGroupChanged() { handleDcOperation(0, InputAction::Change, true); }

void ManualControlViewModel::handleDcOperation(int source, InputAction action, bool grouped)
{
    if (source < 0 || source >= 3)
        return;
    if (m_shortCircuit.active() && action != InputAction::PowerOff) {
        emit dcOutputStateChanged(source, dcOutputOn(source));
        MessageService::instance().showWarning("Short circuit retained",
            "Turn all inputs OFF, then use Relay OFF to discharge and release the retained short first.");
        return;
    }
    QVector<int> targets;
    if (grouped)
    {
        for (int i = 0; i < qBound(1, m_instrumentSetupConfig.dcInputs, 3); ++i)
            for (const auto &instrument : m_instrumentSetupConfig.instruments)
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
        QPointer<ManualControlViewModel> alive(this);
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
    const auto cfg = m_instrumentSetupConfig;
    const auto run = m_operations.dcInput;
    const auto runGroup = m_operations.dcGroup;
    QVector<OutputOperationState::Channel> channels;
    for (int i : targets) channels.append({TableKind::Dc, i});
    const auto ticket = m_outputs.begin(channels, action == InputAction::Change ? std::nullopt
                                                        : std::optional<bool>(action == InputAction::PowerOn));
    QPointer<ManualControlViewModel> alive(this);
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
            QPointer<ManualControlViewModel> alive(this);
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

void ManualControlViewModel::refreshDcInputs()
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

void ManualControlViewModel::rejectOperation(const QString &title, const QString &message, TableKind type)
{
    QPointer<ManualControlViewModel> alive(this);
    emit forceOff(type);
    if (alive)
        emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
    if (alive)
        MessageService::instance().showWarning(title, message);
}

void ManualControlViewModel::handleInput(InputAction action)
{
    if (!m_controlAllowed)
        return;
    if (m_shortCircuit.active() && action != InputAction::PowerOff) {
        rejectOperation("Short circuit retained", "Turn all inputs OFF, then use Relay OFF to discharge and release the retained short first.", TableKind::Input);
        return;
    }
    const auto &inputSel = m_selections.value(TableKind::Input);
    auto validResult = InstrumentConfigValidator::validateInput(m_instrumentSetupConfig, inputSel.text);
    if (action != InputAction::PowerOff && !validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Input);
        return;
    }

    const InstrumentSetupConfig cfg = m_instrumentSetupConfig;
    if (action != InputAction::PowerOff && (inputSel.index < 0 || inputSel.index >= m_conditions.inputRows.size()))
    {
        rejectOperation("Input Selection Error", "Selected input row is out of range.", TableKind::Input);
        return;
    }

    const InputRow inputRow = m_conditions.inputRows.value(inputSel.index); // OFF also works without a selected row.
    startInstrumentOperation(
        [cfg, inputRow, action, run = m_operations.input] { return run(cfg, inputRow, action); },
        "Input Configuration Error", TableKind::Input, action == InputAction::PowerOn, false,
        action == InputAction::Change ? std::nullopt : std::optional<bool>(action == InputAction::PowerOn));
}

void ManualControlViewModel::handleRelay(RelayAction action)
{
    if (!m_controlAllowed)
        return;
    if (m_shortCircuit.active()) {
        bool inputsOff = !outputMayBeOn(m_outputs.state(TableKind::Input)) && !m_outputs.busy(TableKind::Input);
        for (int i = 0; i < 3; ++i)
            inputsOff &= !outputMayBeOn(m_outputs.state(TableKind::Dc, i)) && !m_outputs.busy(TableKind::Dc, i);
        if (action != RelayAction::RelayOff || !inputsOff || m_outputs.busy(TableKind::Relay)) {
            rejectOperation("Short circuit retained", "Relay changes are blocked. Confirm all AC/DC inputs OFF before Relay OFF can discharge and release the short.", TableKind::Relay);
            return;
        }
        const auto cfg = m_instrumentSetupConfig;
        const auto protection = m_shortCircuit;
        const auto operations = m_operations;
        startInstrumentOperation([cfg, protection, operations] {
            auto inputOff = [&](QString& error) {
                QStringList errors;
                auto attempt = [&](const auto& operation) {
                    try {
                        const auto result = operation();
                        if (!result.success) errors << (result.errorMessage.isEmpty() ? "Input OFF failed" : result.errorMessage);
                    } catch (const std::exception& ex) { errors << QString::fromUtf8(ex.what()); }
                      catch (...) { errors << "Unexpected Input OFF exception"; }
                };
                if (hasConfiguredAc(cfg)) attempt([&] { return operations.input(cfg, {}, InputAction::PowerOff); });
                if (hasConfiguredDc(cfg)) attempt([&] { return operations.dcGroup(cfg, {}, InputAction::PowerOff); });
                error = errors.join("; ");
                return errors.isEmpty();
            };
            auto relays = [&](bool shorted, bool discharge, QString& error) {
                RelayDataRow row; row.values.fill("OFF", protection.shortMask.size());
                for (int i = 0; i < row.values.size(); ++i)
                    if ((shorted && protection.shortMask[i] == "ON") ||
                        (discharge && protection.dischargeMask.value(i) == "ON")) row.values[i] = "ON";
                const auto result = operations.relay(cfg, {row}, 0, RelayAction::RelayOn);
                error = result.errorMessage;
                return result.success;
            };
            QString error;
            const bool ok = finishShortCircuit(inputOff, relays, protection.shortMayBeOn,
                                              protection.powerOffMs, protection.dischargeMs, error);
            return InstrumentOperationResult{ok, error};
        }, "Short circuit cleanup", TableKind::Relay, false, false, false);
        return;
    }
    const auto &relaySel = m_selections.value(TableKind::Relay);
    auto validResult = InstrumentConfigValidator::validateRelay(m_instrumentSetupConfig, relaySel.text);
    if (action != RelayAction::RelayOff && !validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Relay);
        return;
    }

    const InstrumentSetupConfig cfg = m_instrumentSetupConfig;
    const auto relayRows = m_conditions.relayRows;
    const int condIdx = relaySel.index;
    startInstrumentOperation(
        [cfg, relayRows, condIdx, action, run = m_operations.relay]
        { return run(cfg, relayRows, condIdx, action); }, "Relay Communication Error", TableKind::Relay,
        action == RelayAction::RelayOn, false,
        action == RelayAction::Change ? std::nullopt : std::optional<bool>(action == RelayAction::RelayOn));
}

void ManualControlViewModel::handleLoad(LoadAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &loadSel = m_selections.value(TableKind::Load);
    auto validResult = InstrumentConfigValidator::validateLoad(m_instrumentSetupConfig, loadSel.text);
    if (action != LoadAction::LoadOff && !validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::Load);
        return;
    }
    if (!tryBeginLoadOperation("handleLoad"))
        return;

    const InstrumentSetupConfig cfg = m_instrumentSetupConfig;
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

void ManualControlViewModel::handleDyLoad(DynamicLoadAction action)
{
    if (!m_controlAllowed)
        return;
    const auto &dyLoadSel = m_selections.value(TableKind::DyLoad);
    auto validResult = InstrumentConfigValidator::validateDyLoad(m_instrumentSetupConfig, dyLoadSel.text);
    if (action != DynamicLoadAction::LoadOff && !validResult.isValid)
    {
        rejectOperation(validResult.errorTitle, validResult.errorMessage, TableKind::DyLoad);
        return;
    }
    if (!tryBeginLoadOperation("handleDyLoad"))
        return;

    const bool syncEnabled = m_syncEnabled;
    const bool syncDirty = m_syncDirty;

    const InstrumentSetupConfig cfg = m_instrumentSetupConfig;
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

void ManualControlViewModel::OnWaveformCaptured()
{
    PngCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void ManualControlViewModel::OnCsvCaptured(int channel)
{
    const auto context = buildCaptureContext(channel);
    executeCapture([&context] { executeWaveformCapture(context, WaveformCaptureFormat::Csv); });
}

void ManualControlViewModel::OnAllCsvCaptured()
{
    AllCsvCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void ManualControlViewModel::OnAllWfmCaptured()
{
    AllWfmCaptureCommand cmd(buildCaptureContext());
    executeCapture([&cmd] { cmd.execute(); });
}

void ManualControlViewModel::OnWfmCaptured(int channel)
{
    const auto context = buildCaptureContext(channel);
    executeCapture([&context] { executeWaveformCapture(context, WaveformCaptureFormat::Wfm); });
}

CaptureContext ManualControlViewModel::buildCaptureContext(int channel) const
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

void ManualControlViewModel::onSyncChanged(bool enabled)
{
    ++m_syncRevision;
    m_syncEnabled = enabled;

    if (!enabled)
    {
        m_syncDirty = true;
    }

    qDebug() << "[ManualControlVM] SyncDynamic enabled=" << enabled << "dirty=" << m_syncDirty;
}

void ManualControlViewModel::validateXml(QXmlStreamReader &reader) const
{
    ManualControlModel candidate;
    candidate.loadXml(reader);
}

void ManualControlViewModel::startInstrumentOperation(std::function<InstrumentOperationResult()> work,
                                              const QString &errorTitle, TableKind type,
                                              bool forceOffOnFailure, bool releaseLoadBusy,
                                              std::optional<bool> outputOn)
{
    QPointer<ManualControlViewModel> guard(this);
    const bool loadOff = outputOn && !*outputOn && (type == TableKind::Load || type == TableKind::DyLoad);
    // Only dynamic ON/Change applies the pending sync update; OFF bypasses it.
    const bool appliesPendingSync = type == TableKind::DyLoad && !loadOff && m_syncDirty;
    const auto syncRevision = m_syncRevision;
    const bool shortRecovery = type == TableKind::Relay && m_shortCircuit.active() && outputOn && !*outputOn;
    const auto otherLoad = type == TableKind::Load ? TableKind::DyLoad : TableKind::Load;
    QVector<OutputOperationState::Channel> channels{{type, 0}};
    if (loadOff) channels.append({otherLoad, 0});
    const auto ticket = m_outputs.begin(channels, outputOn);
    updateControlActivity();
    if (!guard)
        return;
    emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
    if (!guard)
        return;
    if (loadOff) emit restoreOutputState(otherLoad, outputMayBeOn(m_outputs.state(otherLoad)));
    if (!guard) return;
    m_operationQueue.submit(
        std::move(work),
        [this, errorTitle, type, forceOffOnFailure, releaseLoadBusy,
         ticket, loadOff, otherLoad, appliesPendingSync, syncRevision, outputOn, shortRecovery](const InstrumentOperationResult &result)
        {
            QPointer<ManualControlViewModel> alive(this);
            // A failed operation or a newer update must remain pending for retry.
            if (result.success && appliesPendingSync && syncRevision == m_syncRevision)
                m_syncDirty = false;
            m_outputs.complete(ticket, result);
            if (type == TableKind::Relay && outputOn && !*outputOn && result.success)
                m_shortCircuit = {};
            if (shortRecovery && !result.success) {
                // The recovery rechecks Input OFF on the worker. A failed recheck invalidates
                // the earlier UI confirmation; retain the short and require explicit OFF again.
                if (hasConfiguredAc(m_instrumentSetupConfig)) {
                    m_outputs.markUnknown({TableKind::Input, 0});
                    emit restoreOutputState(TableKind::Input, true);
                    if (!alive) return;
                }
                if (hasConfiguredDc(m_instrumentSetupConfig)) {
                    for (int i = 0; i < qBound(1, m_instrumentSetupConfig.dcInputs, 3); ++i) {
                        const bool enabled = std::any_of(m_instrumentSetupConfig.instruments.cbegin(), m_instrumentSetupConfig.instruments.cend(),
                            [i](const auto& instrument) { return instrument.enabled && instrument.type == "InputDCSource" &&
                                instrument.name == QString("DC Source%1").arg(i + 1); });
                        if (!enabled) continue;
                        m_outputs.markUnknown({TableKind::Dc, i});
                        emit dcOutputStateChanged(i, true);
                        if (!alive) return;
                    }
                }
            }
            if (!result.success)
            {
                if (forceOffOnFailure)
                    emit forceOff(type);
            }
            if (!alive)
                return;
            emit restoreOutputState(type, outputMayBeOn(m_outputs.state(type)));
            if (alive && loadOff)
                emit restoreOutputState(otherLoad, outputMayBeOn(m_outputs.state(otherLoad)));
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

bool ManualControlViewModel::isControlActive() const { return hasActiveControl() || isConfigurationBusy(); }

bool ManualControlViewModel::scopeControlsEnabled() const
{
    const auto scope = m_scopes.current();
    return m_controlAllowed && !isConfigurationBusy() && !m_scopes.hasPendingConfiguration() &&
           !m_scopes.testBusy() && !m_capturePreparing && !m_scopes.captureBusy() &&
           !m_triggerBinding.hasActiveCommand() && scope && scope->isConnected();
}

bool ManualControlViewModel::hasActiveControl() const
{
    if (m_triggerBinding.hasActiveCommand())
        return true;
    return m_outputs.hasActiveControl() || m_capturePreparing || m_scopes.captureBusy();
}

bool ManualControlViewModel::hasPendingControlOperation() const
{
    return m_outputs.hasPendingOperations() || m_triggerBinding.hasActiveCommand() ||
           m_capturePreparing || m_scopes.captureBusy();
}
void ManualControlViewModel::setControlAllowed(bool allowed)
{
    if (m_controlAllowed == allowed)
        return;
    m_controlAllowed = allowed;
    updateControlActivity();
}
void ManualControlViewModel::updateControlActivity()
{
    QPointer<ManualControlViewModel> alive(this);
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
void ManualControlViewModel::executeCapture(const std::function<void()> &execute)
{
    if (m_scopes.pollingBusy())
        return;
    if (!scopeControlsEnabled())
        return;
    QPointer<ManualControlViewModel> alive(this);
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

void ManualControlViewModel::publishXmlLoaded()
{
    restoreFromModel();
    refreshDcInputs();
    updateUIAfterLoad();
}
