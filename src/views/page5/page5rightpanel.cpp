#include <QSignalBlocker>
#include "page5rightpanel.h"
#include "tableutils.h"
#include "page5style.h"
#include "page5viewmodel.h"
#include "collapsiblesection.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QFont>
#include <QPushButton>

static QLabel* makePanelTitle(const QString& text, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(Page5Style::PANEL_TITLE);
    return label;
}

Page5RightPanel::Page5RightPanel(Page5ViewModel* viewModel, QWidget* parent)
    : SidePanel(Qt::RightEdge, 260, parent), m_viewModel(viewModel)
{
    buildPanel();
}

void Page5RightPanel::buildPanel()
{
    auto* cl = contentLayout();
    cl->addWidget(makePanelTitle("Report"));

    // File
    auto* fileSec = new CollapsibleSection("File", this);
    fileSec->setExpandedDelayed(true);
    m_fileTable = buildFileTable();
    fileSec->contentLayout()->addWidget(m_fileTable);
    cl->addWidget(fileSec);

    cl->addStretch();
}

QTableWidget* Page5RightPanel::buildFileTable()
{
    auto* table = new QTableWidget(2, 2);
    table->setHorizontalHeaderLabels({"Property", "Value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Row 0：File Name
    auto* labelItem0 = new QTableWidgetItem("File Name");
    labelItem0->setFlags(Qt::ItemIsEnabled);
    labelItem0->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    labelItem0->setFont(QFont("Arial", 9, QFont::Bold));
    table->setItem(0, 0, labelItem0);

    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("Enter file name…");
    nameEdit->setFrame(false);
    nameEdit->setContentsMargins(4, 0, 4, 0);
    nameEdit->setStyleSheet("QLineEdit { background: #ffffff; color: #222222; border: none; }");
    table->setCellWidget(0, 1, nameEdit);

    // Row 1：File Path
    auto* labelItem1 = new QTableWidgetItem("File Path");
    labelItem1->setFlags(Qt::ItemIsEnabled);
    labelItem1->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    labelItem1->setFont(QFont("Arial", 9, QFont::Bold));
    table->setItem(1, 0, labelItem1);

    auto* cell = new QWidget;
    auto* hlay = new QHBoxLayout(cell);
    hlay->setContentsMargins(2, 1, 2, 1);
    hlay->setSpacing(2);

    auto* pathEdit = new QLineEdit;
    pathEdit->setPlaceholderText("Select path…");
    pathEdit->setFrame(false);
    pathEdit->setReadOnly(true);
    pathEdit->setStyleSheet(R"(
        QLineEdit { background: #ffffff; color: #222222; border: none; }
        QLineEdit:focus { border: none; outline: none; })");

    auto* browseBtn = new QPushButton("…");
    browseBtn->setFixedWidth(24);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setToolTip("Browse folder");
    browseBtn->setStyleSheet(R"(
        QPushButton {
            border: 1px solid #aab4c8;
            border-radius: 3px;
            background: #f0f3f8;
            font-size: 12px;
            padding: 0px;
        }
        QPushButton:hover   { background: #dde8ff; border-color: #2a6dd9; }
        QPushButton:pressed { background: #c8d8ff; }
    )");

    connect(browseBtn, &QPushButton::clicked, pathEdit, [pathEdit, this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Select Directory", pathEdit->text().isEmpty() ? QDir::homePath() : pathEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty())
            pathEdit->setText(dir);
    });

    hlay->addWidget(pathEdit, 1);
    hlay->addWidget(browseBtn, 0);
    table->setCellWidget(1, 1, cell);

    nameEdit->setText(m_viewModel->reportName());
    pathEdit->setText(m_viewModel->reportDirectory());
    auto updateFile = [this, nameEdit, pathEdit] {
        m_viewModel->setReportFile(nameEdit->text(), pathEdit->text());
    };
    connect(nameEdit, &QLineEdit::textChanged, this, updateFile);
    connect(pathEdit, &QLineEdit::textChanged, this, updateFile);
    connect(m_viewModel, &Page5ViewModel::reportFileChanged, table, [this, nameEdit, pathEdit] {
        const QSignalBlocker nameBlock(nameEdit), pathBlock(pathEdit);
        nameEdit->setText(m_viewModel->reportName());
        pathEdit->setText(m_viewModel->reportDirectory());
    });
    connect(m_viewModel, &Page5ViewModel::runningChanged, table,
            [table](bool running) { table->setEnabled(!running); });
    constexpr int fileRowHeight = 36;
    table->setRowHeight(0, fileRowHeight);
    table->setRowHeight(1, fileRowHeight);
    const int headerH = table->horizontalHeader()->sizeHint().height();
    table->setFixedHeight(headerH + fileRowHeight * 2 + 4);

    TableUtils::applyTableStyle(table, Page5Style::TABLE);
    return table;
}
