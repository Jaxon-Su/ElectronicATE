#pragma once
#include <QDialog>

class CommScanViewModel;
class QTableWidget;
class QLabel;
class QPushButton;

class CommScanDialog : public QDialog {
    Q_OBJECT
  public:
    explicit CommScanDialog(CommScanViewModel *viewModel, QWidget *parent = nullptr);
    void done(int result) override;
  private:
    void refresh();
    CommScanViewModel *m_viewModel;
    QTableWidget *m_table;
    QLabel *m_status;
    QPushButton *m_copy;
};
