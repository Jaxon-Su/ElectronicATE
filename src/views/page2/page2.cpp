#include "page2.h"
#include "conditionrowsview.h"
#include "groupedconditionsview.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

Page2::Page2(Page2ViewModel *viewModel, QWidget *parent) : QWidget(parent), vm(viewModel)
{
    if (vm->maxOutput() < 1)
        vm->setMaxOutput(1);
    if (vm->maxRelayOutput() < 1)
        vm->setMaxRelayOutput(1);
    tblInput = new ConditionRowsView(vm, TableKind::Input, this);
    tblRelay = new ConditionRowsView(vm, TableKind::Relay, this);
    tblDc = new GroupedConditionsView(vm, TableKind::Dc, this);
    tblLoad = new GroupedConditionsView(vm, TableKind::Load, this);
    tblDynamic = new GroupedConditionsView(vm, TableKind::DyLoad, this);
    auto *left = new QVBoxLayout;
    auto *right = new QVBoxLayout;
    auto add = [this](QVBoxLayout *layout, auto *table, TableKind kind) {
        auto *buttons = new QHBoxLayout;
        auto *plus = new QPushButton("+", this), *minus = new QPushButton("-", this);
        buttons->addWidget(plus);
        buttons->addWidget(minus);
        layout->addLayout(buttons);
        layout->addWidget(table);
        connect(plus, &QPushButton::clicked, vm, [this, kind] { vm->addRow(kind); });
        connect(minus, &QPushButton::clicked, vm, [this, kind] { vm->removeRow(kind); });
        connect(vm, &Page2ViewModel::rowAddRequested, table,
                [table, kind](TableKind requested, const QStringList &) {
                    if (requested == kind)
                        table->appendRow();
                });
        connect(vm, &Page2ViewModel::rowRemoveRequested, table, [table, kind](TableKind requested) {
            if (requested == kind)
                table->removeSelectedRows();
        });
    };
    add(left, tblInput, TableKind::Input);
    add(left, tblDc, TableKind::Dc);
    add(left, tblRelay, TableKind::Relay);
    add(right, tblLoad, TableKind::Load);
    add(right, tblDynamic, TableKind::DyLoad);
    auto *layout = new QHBoxLayout(this);
    layout->addLayout(left, 1);
    layout->addLayout(right, 3);
    connect(vm, &Page2ViewModel::dataChanged, this, &Page2::resetUIFromViewModel);
    auto snapshot = vm->conditions();
    GroupedConditionsModel::normalizeDc(snapshot);
    vm->setConditions(snapshot);
    if (snapshot.dcRows.isEmpty())
        tblDc->appendRow();
    if (snapshot.relayRows.isEmpty())
        tblRelay->appendRow();
    if (snapshot.loadRows.isEmpty())
        tblLoad->appendRow();
    if (snapshot.dynamicRows.isEmpty())
        tblDynamic->appendRow();
}

void Page2::syncUIToViewModel()
{
    // Editors commit through their model. Never reconstruct a snapshot from widgets.
    auto snapshot = vm->conditions();
    GroupedConditionsModel::normalizeDc(snapshot);
    vm->setConditions(snapshot);
    emit conditionsEdited(snapshot);
}
void Page2::resetUIFromViewModel()
{
    tblInput->refreshRows();
    tblRelay->refreshRows();
    tblDc->refreshRows();
    tblLoad->refreshRows();
    tblDynamic->refreshRows();
}
