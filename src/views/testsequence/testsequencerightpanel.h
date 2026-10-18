#pragma once
#include "sidepanel.h"

class TestSequenceViewModel;

// TestSequence Excel report destination controls.
class TestSequenceRightPanel : public SidePanel {
    Q_OBJECT
  public:
    explicit TestSequenceRightPanel(TestSequenceViewModel* viewModel, QWidget* parent = nullptr);
    ~TestSequenceRightPanel() override = default;

  private:
    void buildPanel();
    QWidget* buildFileForm();
    TestSequenceViewModel* m_viewModel = nullptr;
};
