#pragma once
#include "testconditionsviewmodel.h"
#include <QWidget>
class ConditionRowsView;
class GroupedConditionsView;
class TestConditionsPage : public QWidget
{
    Q_OBJECT
  public:
    explicit TestConditionsPage(TestConditionsViewModel *viewModel, QWidget *parent = nullptr);
    void syncUIToViewModel();
  signals:
    void conditionsEdited(const TestConditionSnapshot &snapshot);
  private slots:
    void resetUIFromViewModel();

  private:
    TestConditionsViewModel *vm;
    ConditionRowsView *tblInput, *tblRelay;
    GroupedConditionsView *tblDc, *tblLoad, *tblDynamic;
};
