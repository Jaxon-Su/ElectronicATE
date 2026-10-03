
#pragma once
#include "loadsyncsettings.h"

#include <QObject>
#include <QStringList>
#include <QMap>
#include "page3model.h"
#include "page1config.h"
#include "page2config.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "oscilloscope.h"
#include "triggerbinding.h"

#include "capturecontext.h"
#include "ixmlserializable.h"
#include "tableheaderbuilder.h"
#include "scopesessioncoordinator.h"
#include "outputoperationstate.h"
#include "instrumentoperationqueue.h"
#include "page3operations.h"
#include <QPointer>

#include <memory>
#include <optional>
#include <utility>

// InputAction / LoadAction / DynamicLoadAction / RelayAction
// Defined in instrumentactions.h, shared with hardware execution services.

// 各 TableKind 的選擇狀態（index + text）
struct SelectionState {
    int index = -1;
    QString text;
};

class Page3ViewModel : public QObject, public IXmlSerializable {
    Q_OBJECT
  public:
    Page3ViewModel(Page3Model* p3, QObject* parent = nullptr);
    Page3ViewModel(Page3Model* model, Page3Operations operations, QObject* parent = nullptr);
    virtual ~Page3ViewModel();

    void setCaptureFileSelector(CaptureFileSelector selector) { m_captureFileSelector = std::move(selector); }

    bool isControlActive() const;
    std::shared_ptr<Oscilloscope> borrowOscilloscope();
    LoadSyncSettings loadSyncSettings() const { return {m_syncEnabled, m_syncDirty}; }
    bool hasActiveControl() const;
    bool hasPendingControlOperation() const;
    bool hasUnknownOutput() const { return m_outputs.hasUnknownOutput(); }
    bool outputBusy(TableKind type) const { return m_outputs.busy(type); }
    bool isConfigurationBusy() const { return m_scopes.isConfigurationBusy(); }
    bool scopeControlsEnabled() const;
    OutputState outputState(TableKind type) const { return m_outputs.state(type); }
    OutputState dcOutputState(int source) const
    {
        return source >= 0 && source < 3 ? m_outputs.state(TableKind::Dc, source) : OutputState::Unknown;
    }
    void setControlAllowed(bool allowed);
    bool dcOutputOn(int source) const { return source >= 0 && source < 3 && outputMayBeOn(m_outputs.state(TableKind::Dc, source)); }
    bool dcBusy(int source) const { return source >= 0 && source < 3 && m_outputs.busy(TableKind::Dc, source); }

    // IXmlSerializable
    QString xmlTagName() const override { return "Page3"; }
    void writeXml(QXmlStreamWriter& writer) const override;
    void publishXmlLoaded() override;
    void validateXml(QXmlStreamReader& reader) const override;
    void loadXml(QXmlStreamReader& reader) override;

    void restoreFromModel();
    void updateUIAfterLoad();
    void restoreUISelections();

    // 獲取當前選擇狀態的方法（透過 SelectionState map）
    int getSelectedInputIndex() const { return m_selections.value(TableKind::Input).index; }
    QString getSelectedInputText() const { return m_selections.value(TableKind::Input).text; }
    int getSelectedLoadIndex() const { return m_selections.value(TableKind::Load).index; }
    QString getSelectedLoadText() const { return m_selections.value(TableKind::Load).text; }
    int getSelectedDyLoadIndex() const { return m_selections.value(TableKind::DyLoad).index; }
    QString getSelectedDyLoadText() const { return m_selections.value(TableKind::DyLoad).text; }
    int getSelectedRelayIndex() const { return m_selections.value(TableKind::Relay).index; }
    QString getSelectedRelayText() const { return m_selections.value(TableKind::Relay).text; }

  public slots:
    void onDcSelected(int source, int index, const QString& text);
    void onDcInputToggled(int source, bool on);
    void onDcInputChanged(int source);
    void onDcGroupToggled(bool on);
    void onDcGroupChanged();
    void handleDcInput(int source, InputAction action);
    void refreshDcInputs();
    void onConditionsChanged(const TestConditionSnapshot& snapshot);
    void setMaxOutput(int maxOutput);
    void setNameList(const QStringList& names);
    void updateTitles(TableKind, const QStringList& titles);

    // Page1/Page2 數據處理
    void onPage1ConfigChanged(const Page1Config& cfg);
    void onInputDataChanged(const QVector<InputRow>& rows);
    void onLoadMetaChanged(const LoadMetaRow& meta);
    void onLoadRowsChanged(const QVector<LoadDataRow>& rows);
    void onDynamicMetaChanged(const DynamicMetaRow& meta);
    void onDynamicRowsChanged(const QVector<DynamicDataRow>& rows);
    void onRelayRowsChanged(const QVector<RelayDataRow>& rows);

    // Input 相關操作
    void onInputToggled(bool on);
    void onInputChanged();
    void handleInput(InputAction action);

    // Load 相關操作
    void onLoadToggled(bool on);
    void onLoadChanged();
    void handleLoad(LoadAction action);

    // Dynamic Load 相關操作
    void onDyloadToggled(bool on);
    void onDyLoadChanged();
    void handleDyLoad(DynamicLoadAction action);

    // Relay 相關操作
    void onRelayToggled(bool on);
    void onRelayChanged();
    void handleRelay(RelayAction action);

    // 選擇處理
    void onSelected(TableKind type, int idx, const QString& txt);

    // Trigger 相關
    void onTriggerWidgetCreated(const QString& modelName, QObject* triggerController);
    void onTriggerWidgetDestroyed();

    // 示波器抓取相關
    void OnWaveformCaptured();
    void OnCsvCaptured(int channel);
    void OnAllCsvCaptured();
    void OnWfmCaptured(int channel);
    void OnAllWfmCaptured();

    // Synchronous Dynamic
    void onSyncChanged(bool enabled);

    // 示波器重連（通訊中斷後使用現有 Page1 設定重建連線）
    void reconnectOscilloscopes();

  private:
    bool m_controlAllowed = true;
    bool m_hardwareControlActive = false;
    bool m_configurationBusy = false;
    bool m_scopeControlsEnabled = false;
    int m_capturePreparing = 0;
    OutputOperationState m_outputs;
    void updateControlActivity();
    void handleDcOperation(int source, InputAction action, bool grouped);
    void executeCapture(const std::function<void()>& execute);

    // UI 狀態

    QStringList m_inputTitles;
    QStringList m_loadTitles;
    QStringList m_dyloadTitles;
    QStringList m_relayTitles;

    // 數據模型
    Page3Model* m_model = nullptr;

    // 配置數據
    Page1Config m_page1Config;
    TestConditionSnapshot m_conditions;

    // 選擇狀態（TableKind → index/text）
    QMap<TableKind, SelectionState> m_selections;

    // 示波器生命週期（含建立、disconnect、current 管理）
    ScopeSessionCoordinator m_scopes;
    TriggerBinding m_triggerBinding;

    // 私有方法
    void connectTriggerController();
    void cleanupTriggerResources();

    // Sync Dynamic 狀態
    // m_syncEnabled : checkbox is currently hidden, so sync dynamic is enabled by default.
    bool m_syncEnabled = true;

    // m_syncDirty : keep compatibility if the optional sync checkbox is restored.
    bool m_syncDirty = false;

    // DC Load hardware operations must not overlap. Page3 runs them in worker
    // threads, so a quick ON/OFF click can otherwise race two command streams.
    bool m_loadOperationBusy = false;
    Page3Operations m_operations;
    InstrumentOperationQueue m_operationQueue;
    bool tryBeginLoadOperation(const char* context);
    void finishLoadOperation();
    void rejectOperation(const QString& title, const QString& message, TableKind type);
    void startInstrumentOperation(std::function<InstrumentOperationResult()> work, const QString& errorTitle,
                                  TableKind type, bool forceOffOnFailure, bool releaseLoadBusy = false,
                                  std::optional<bool> outputOn = {});

    CaptureFileSelector m_captureFileSelector;

    CaptureContext buildCaptureContext(int channel = 0) const;

  signals:
    void dcOutputStateChanged(int source, bool on);
    void dcOperationBusyChanged(int source, bool busy);
    void dcInputUpdated(int source, const QStringList& titles, int selectedIndex);
    void controlActiveChanged(bool active);
    void scopeControlsEnabledChanged(bool enabled);
    void headersChanged(const QStringList& hdr);
    void rowLabelsChanged(const QStringList& lbl);
    void page1ConfigChanged(const Page1Config& cfg);
    void titlesUpdated(TableKind type, const QStringList& titles);
    void forceOff(TableKind type);
    void restoreOutputState(TableKind type, bool on);
    void restoreSelections(TableKind type, int index, const QString& text);
    void loadOperationBusyChanged(bool busy);
};
