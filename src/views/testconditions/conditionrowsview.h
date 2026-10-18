#pragma once
#include <QTableView>
#include "conditionrowsmodel.h"

class ConditionRowsView : public QTableView
{
  public:
    ConditionRowsView(TestConditionsViewModel *vm, TableKind kind, QWidget *parent = nullptr);
    void refreshRows() { m_rows->refreshFromSource(); }
    void appendRow() { m_rows->append(); }
    void removeSelectedRows();

  private:
    void copyRows();
    void pasteRows();
    ConditionRowsModel *m_rows;
    QVector<QStringList> m_clipboard;
};
