#include "commscandialog.h"
#include "../../../viewmodels/page1/commscanviewmodel.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

CommScanDialog::CommScanDialog(CommScanViewModel *viewModel, QWidget *parent)
    : QDialog(parent), m_viewModel(viewModel)
{
    setWindowTitle("Scan Resources");
    resize(900, 480);
    setMinimumSize(700, 360);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    auto *hint = new QLabel(
        "Available VISA resources and system COM ports. No device commands are sent.\n"
        "COM ports do not identify Modbus devices. TCP/IP resources depend on VISA discovery/configuration.", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_table = new QTableWidget(0, 4, this);
    m_table->setObjectName("commScanResults");
    m_table->setHorizontalHeaderLabels({"Communication", "Address", "Source", "Description"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 110);
    m_table->setColumnWidth(1, 250);
    m_table->setColumnWidth(2, 125);
    layout->addWidget(m_table, 1);
    m_status = new QLabel(this);
    m_status->setObjectName("commScanStatus");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    auto *scan = new QPushButton("Scan", this);
    scan->setObjectName("scanCommStart");
    auto *stop = new QPushButton("Stop", this);
    stop->setObjectName("scanCommStop");
    stop->setEnabled(false);
    m_copy = new QPushButton("Copy Address", this);
    m_copy->setToolTip("Copy the address; merged serial resources copy the COM port name.");
    m_copy->setEnabled(false);
    auto *close = new QPushButton("Close", this);
    buttons->addWidget(scan);
    buttons->addWidget(stop);
    buttons->addStretch();
    buttons->addWidget(m_copy);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(scan, &QPushButton::clicked, m_viewModel, &CommScanViewModel::start);
    connect(stop, &QPushButton::clicked, m_viewModel, &CommScanViewModel::stop);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_copy, &QPushButton::clicked, this, [this] {
        if (auto *address = m_table->item(m_table->currentRow(), 1))
            QApplication::clipboard()->setText(address->data(Qt::UserRole).toString());
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { m_copy->setEnabled(!m_table->selectedItems().isEmpty()); });
    connect(viewModel, &CommScanViewModel::entriesChanged, this, &CommScanDialog::refresh);
    connect(viewModel, &CommScanViewModel::statusChanged, m_status, &QLabel::setText);
    connect(viewModel, &CommScanViewModel::runningChanged, this, [scan, stop](bool running) {
        scan->setEnabled(!running);
        stop->setEnabled(running);
    });
    QTimer::singleShot(0, this, [this] {
        if (isVisible()) m_viewModel->start();
    });
}

void CommScanDialog::done(int result)
{
    m_viewModel->stop();
    QDialog::done(result);
}

void CommScanDialog::refresh()
{
    const auto &entries = m_viewModel->entries();
    m_table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const auto &entry = entries[row];
        const QStringList columns{entry.transport, entry.displayAddress(), entry.source, entry.description};
        for (int column = 0; column < columns.size(); ++column) {
            auto *item = m_table->item(row, column);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, column, item);
            }
            item->setText(columns[column]);
            item->setToolTip(columns[column]);
            if (column == 1)
                item->setData(Qt::UserRole, entry.address);
        }
    }
    m_table->resizeRowsToContents();
}
