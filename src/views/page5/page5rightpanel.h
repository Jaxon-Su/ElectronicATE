#pragma once
#include <QTableWidget>
#include <QLineEdit>
#include "sidepanel.h"

class Page5ViewModel;

// Page5 Excel report destination controls.
class Page5RightPanel : public SidePanel {
    Q_OBJECT
  public:
    explicit Page5RightPanel(Page5ViewModel* viewModel, QWidget* parent = nullptr);
    ~Page5RightPanel() override = default;

  private:
    void buildPanel();
    QTableWidget* buildFileTable();
    Page5ViewModel* m_viewModel = nullptr;
    QTableWidget* m_fileTable = nullptr;
};
