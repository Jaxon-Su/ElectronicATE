#include "pageconnectioncoordinator.h"
#include "instrumentsetupviewmodel.h"
#include "testconditionsviewmodel.h"
#include "manualcontrolviewmodel.h"
#include "testsequenceviewmodel.h"
#include <QDebug>
#include <QPointer>

void PageConnectionCoordinator::setupPageConnections(InstrumentSetupViewModel* instrumentSetup, TestConditionsViewModel* testConditions,
                                                     ManualControlViewModel* manualControl, TestSequenceViewModel* testSequence)
{
    if (!instrumentSetup || !testConditions || !manualControl || !testSequence) {
        qWarning() << "PageConnectionCoordinator::setupPageConnections - null viewmodel pointer";
        return;
    }

    testSequence->setLoadSyncProvider([owner = QPointer<ManualControlViewModel>(manualControl)] {
        return owner ? owner->loadSyncSettings() : LoadSyncSettings{};
    });
    testSequence->setOscilloscopeProvider([owner = QPointer<ManualControlViewModel>(manualControl)] {
        return owner ? owner->borrowOscilloscope() : std::shared_ptr<Oscilloscope>{};
    });

    QObject::connect(testSequence, &TestSequenceViewModel::outputsUncertain, manualControl, &ManualControlViewModel::adoptUncertainOutputs);
    testSequence->setOutputRecoveryCheck([owner = QPointer<ManualControlViewModel>(manualControl)] {
        return owner && !owner->hasActiveControl();
    });

    connectInstrumentSetupToTestConditions(instrumentSetup, testConditions);
    connectInstrumentSetupToManualControl(instrumentSetup, manualControl);
    connectTestConditionsToManualControl(testConditions, manualControl);
    connectInstrumentSetupToTestSequence(instrumentSetup, testSequence);
    connectTestConditionsToTestSequence(testConditions, testSequence);
}

void PageConnectionCoordinator::connectInstrumentSetupToTestConditions(InstrumentSetupViewModel* instrumentSetup, TestConditionsViewModel* testConditions)
{
    // InstrumentSetup load 數量連動 TestConditions
    QObject::connect(instrumentSetup, &InstrumentSetupViewModel::loadOutputsChanged, testConditions, &TestConditionsViewModel::setMaxOutput);

    QObject::connect(instrumentSetup, &InstrumentSetupViewModel::relayOutputsChanged, testConditions, &TestConditionsViewModel::setMaxRelayOutput);

    QObject::connect(instrumentSetup, &InstrumentSetupViewModel::configUpdated, testConditions, &TestConditionsViewModel::onInstrumentSetupConfigChanged);
}

void PageConnectionCoordinator::connectInstrumentSetupToManualControl(InstrumentSetupViewModel* instrumentSetup, ManualControlViewModel* manualControl)
{
    // InstrumentSetup 連動 ManualControl
    QObject::connect(instrumentSetup, &InstrumentSetupViewModel::configUpdated, manualControl, &ManualControlViewModel::onInstrumentSetupConfigChanged);
}

void PageConnectionCoordinator::connectTestConditionsToManualControl(TestConditionsViewModel* testConditions, ManualControlViewModel* manualControl)
{
    // TestConditions tblinput 連動 ManualControl
    QObject::connect(testConditions, &TestConditionsViewModel::titleListChanged, manualControl, &ManualControlViewModel::updateTitles);

    QObject::connect(testConditions, &TestConditionsViewModel::conditionsChanged, manualControl, &ManualControlViewModel::onConditionsChanged);
}
void PageConnectionCoordinator::connectInstrumentSetupToTestSequence(InstrumentSetupViewModel* instrumentSetup, TestSequenceViewModel* testSequence)
{
    // InstrumentSetup 儀器通訊設定 連動 TestSequence
    QObject::connect(instrumentSetup, &InstrumentSetupViewModel::configUpdated, testSequence, &TestSequenceViewModel::onInstrumentSetupConfigChanged);
}

void PageConnectionCoordinator::connectTestConditionsToTestSequence(TestConditionsViewModel* testConditions, TestSequenceViewModel* testSequence)
{
    // 先注入委託指標，確保 TestSequenceViewModel 的 accessor 在 signal 抵達前已就緒
    testSequence->setConditionProvider(testConditions);
    QObject::connect(testConditions, &TestConditionsViewModel::conditionsChanged, testSequence, &TestSequenceViewModel::dcTableChanged);

    // TestConditions 各條件 Table 連動 TestSequence（signal 只觸發 UI 刷新，不傳資料副本）
    QObject::connect(testConditions, &TestConditionsViewModel::inputRowsStructChanged, testSequence, &TestSequenceViewModel::onInputDataChanged);
    QObject::connect(testConditions, &TestConditionsViewModel::loadMetaStructChanged, testSequence, &TestSequenceViewModel::onLoadMetaChanged);
    QObject::connect(testConditions, &TestConditionsViewModel::loadRowsStructChanged, testSequence, &TestSequenceViewModel::onLoadRowsChanged);
    QObject::connect(testConditions, &TestConditionsViewModel::dynamicMetaStructChanged, testSequence,
                     &TestSequenceViewModel::onDynamicMetaChanged);
    QObject::connect(testConditions, &TestConditionsViewModel::dynamicRowsStructChanged, testSequence,
                     &TestSequenceViewModel::onDynamicRowsChanged);
    QObject::connect(testConditions, &TestConditionsViewModel::relayRowsStructChanged, testSequence, &TestSequenceViewModel::onRelayRowsChanged);
}
