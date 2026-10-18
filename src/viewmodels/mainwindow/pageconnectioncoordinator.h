#pragma once

class InstrumentSetupViewModel;
class TestConditionsViewModel;
class ManualControlViewModel;
class TestSequenceViewModel;

// Presentation composition: wire page dependencies after constructing the VMs.
// Connections use receiver lifetimes; this coordinator owns no page state.
class PageConnectionCoordinator final {
  public:
    static void setupPageConnections(InstrumentSetupViewModel* instrumentSetup, TestConditionsViewModel* testConditions, ManualControlViewModel* manualControl,
                                     TestSequenceViewModel* testSequence);

  private:
    PageConnectionCoordinator() = delete;
    static void connectInstrumentSetupToTestConditions(InstrumentSetupViewModel* instrumentSetup, TestConditionsViewModel* testConditions);
    static void connectInstrumentSetupToManualControl(InstrumentSetupViewModel* instrumentSetup, ManualControlViewModel* manualControl);
    static void connectTestConditionsToManualControl(TestConditionsViewModel* testConditions, ManualControlViewModel* manualControl);
    static void connectInstrumentSetupToTestSequence(InstrumentSetupViewModel* instrumentSetup, TestSequenceViewModel* testSequence);
    static void connectTestConditionsToTestSequence(TestConditionsViewModel* testConditions, TestSequenceViewModel* testSequence);
};
