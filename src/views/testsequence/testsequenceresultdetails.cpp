#include "testsequenceresultdetails.h"
#include "../../ui/style/testsequencestyle.h"
#include "../../viewmodels/testsequence/testsequenceresultsmodel.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>

TestSequenceResultDetails::TestSequenceResultDetails(TestSequenceResultsModel *model, QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    auto *filterBar = new QFrame(this);
    filterBar->setObjectName("resultFilterBar");
    filterBar->setStyleSheet(TestSequenceStyle::RESULT_FILTERS);
    auto *filters = new QHBoxLayout(filterBar);
    filters->setContentsMargins(8, 4, 8, 4);
    filters->setSpacing(6);
    auto *mode = new QComboBox(filterBar);
    mode->setObjectName("resultDetailMode");
    mode->addItems({tr("Summary"), tr("Trials")});
    mode->setToolTip(tr("Per channel: Rise selects the highest Max; Fall selects the lowest Min. "
                       "All four values come from the same record. Ties keep the earlier record. "
                       "Turn off uses a labeled steady reference when a direction has no event. "
                       "Turn off (descent) defaults to Trials, showing each Level / Trial."));
    filters->addWidget(mode);
    connect(mode, &QComboBox::currentIndexChanged, model, [model](int index) { model->setSummary(index == 0); });
    const auto syncMode = [mode](bool summary) {
        QSignalBlocker blocked(mode);
        mode->setCurrentIndex(summary ? 0 : 1);
    };
    connect(model, &TestSequenceResultsModel::summaryChanged, mode, syncMode);
    syncMode(model->summary());
    for (const auto &entry : QList<QPair<QString, int>>{{tr("Attempt"), TestSequenceResultsModel::AttemptFilter},
                                                      {tr("Load"), TestSequenceResultsModel::LoadFilter},
                                                      {tr("Direction"), TestSequenceResultsModel::DirectionFilter},
                                                      {tr("CH"), TestSequenceResultsModel::ChannelFilter}}) {
        auto *filter = new QComboBox(filterBar);
        filter->setObjectName(QString("resultFilter%1").arg(entry.second));
        filter->setMinimumContentsLength(6);
        filter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        filters->addWidget(new QLabel(entry.first, filterBar));
        filters->addWidget(filter);
        const auto refresh = [model, filter, column = entry.second] {
            const auto selected = model->filterValue(column);
            QSignalBlocker blocked(filter);
            filter->clear(); filter->addItem(QObject::tr("All"), "");
            for (const auto &value : model->options(column)) filter->addItem(value, value);
            filter->setCurrentIndex(qMax(0, filter->findData(selected)));
        };
        connect(model, &QAbstractItemModel::modelReset, filter, refresh);
        connect(filter, &QComboBox::currentIndexChanged, model, [model, filter, column = entry.second] {
            model->setFilter(column, filter->currentData().toString());
        });
        refresh();
    }
    filters->addStretch();
    layout->addWidget(filterBar);
    auto *table = new QTableView(this);
    table->setObjectName("taskResultDetails");
    table->setModel(model);
    table->setStyleSheet(TestSequenceStyle::RESULT_TABLE);
    table->horizontalHeader()->setObjectName("resultDetailHeader");
    table->horizontalHeader()->setStyleSheet(TestSequenceStyle::RESULT_HEADER);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(30);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setSectionsMovable(true);
    table->horizontalHeader()->setDefaultSectionSize(90);
    for (int column : {0, 1, 2, 10}) table->hideColumn(column);
    table->setColumnWidth(9, 220);
    table->setColumnWidth(TestSequenceResultsModel::LoadFilter, 170);
    auto *header = table->horizontalHeader();
    header->moveSection(header->visualIndex(TestSequenceResultsModel::LoadFilter), 0);
    header->moveSection(header->visualIndex(TestSequenceResultsModel::DirectionFilter), 1);
    layout->addWidget(table);
}
