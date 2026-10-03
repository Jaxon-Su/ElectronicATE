#pragma once
#include "page2viewmodel.h"
#include <QWidget>
class ConditionRowsView;
class GroupedConditionsView;
class Page2 : public QWidget
{
    Q_OBJECT
  public:
    explicit Page2(Page2ViewModel *viewModel, QWidget *parent = nullptr);
    void syncUIToViewModel();
  signals:
    void conditionsEdited(const TestConditionSnapshot &snapshot);
  private slots:
    void resetUIFromViewModel();

  private:
    Page2ViewModel *vm;
    ConditionRowsView *tblInput, *tblRelay;
    GroupedConditionsView *tblDc, *tblLoad, *tblDynamic;
};
