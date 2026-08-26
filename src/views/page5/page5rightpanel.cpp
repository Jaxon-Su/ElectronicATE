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

// ── 建立 Panel Title Label ────────────────────────────
static QLabel* makePanelTitle(const QString& text, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(Page5Style::PANEL_TITLE);
    return label;
}

Page5RightPanel::Page5RightPanel(Page5ViewModel* viewModel, QWidget* parent)
    : SidePanel(Qt::RightEdge, 260, parent)
    , m_viewModel(viewModel)
{
    buildPanel();
}

void Page5RightPanel::buildPanel()
{
    auto* cl = contentLayout();
    cl->addWidget(makePanelTitle("Conditions"));

    // ── File ──
    auto* fileSec = new CollapsibleSection("File", this);
    fileSec->setExpandedDelayed(false);
    m_fileTable = buildFileTable();
    fileSec->contentLayout()->addWidget(m_fileTable);
    cl->addWidget(fileSec);

    // ── Input Table ──
    auto* inputSec = new CollapsibleSection("Input Table", this);
    inputSec->setExpandedDelayed(true);
    m_inputTable = buildInputTableWidget();
    inputSec->contentLayout()->addWidget(m_inputTable);
    cl->addWidget(inputSec);

    // ── Load Table ──
    auto* loadSec = new CollapsibleSection("Load Table", this);
    loadSec->setExpandedDelayed(false);
    m_loadTable = buildLoadTableWidget();
    loadSec->contentLayout()->addWidget(m_loadTable);
    cl->addWidget(loadSec);

    // ── Dynamic Table ──
    auto* dynSec = new CollapsibleSection("Dynamic Table", this);
    dynSec->setExpandedDelayed(false);
    m_dynamicTable = buildDynamicTableWidget();
    dynSec->contentLayout()->addWidget(m_dynamicTable);
    cl->addWidget(dynSec);

    // ── Relay Table ──
    auto* relaySec = new CollapsibleSection("Relay Table", this);
    relaySec->setExpandedDelayed(false);
    m_relayTable = buildRelayTableWidget();
    relaySec->contentLayout()->addWidget(m_relayTable);
    cl->addWidget(relaySec);

    cl->addStretch();
}

// ══════════════════════════════════════════════════════
//  表格建立
// ══════════════════════════════════════════════════════
QTableWidget* Page5RightPanel::makeReadOnlyTable(const QStringList& headers,
                                                 int minH, int maxH)
{
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    t->horizontalHeader()->setMinimumSectionSize(60);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    t->setMinimumHeight(minH);
    t->setMaximumHeight(maxH);
    TableUtils::applyTableStyle(t, Page5Style::TABLE);
    return t;
}

QTableWidget* Page5RightPanel::buildInputTableWidget()
{
    // ★ 新增 "Input" 欄位（格式：Vin/Frequency/Phase），與 Page2 一致
    auto* t = makeReadOnlyTable({"Seq", "Input", "Vin", "Frequency", "Phase"}, 44, 200);
    t->horizontalHeader()->resizeSection(0, 36);   // Seq
    t->horizontalHeader()->resizeSection(1, 80);   // Input
    return t;
}

QTableWidget* Page5RightPanel::buildLoadTableWidget()
{
    return makeReadOnlyTable({"Output", "Index1"}, 44, 300);
}

QTableWidget* Page5RightPanel::buildDynamicTableWidget()
{
    return makeReadOnlyTable({"Output", "Index1"}, 44, 300);
}

QTableWidget* Page5RightPanel::buildRelayTableWidget()
{
    return makeReadOnlyTable({"Relay", "Index1"}, 44, 200);
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

    auto* cell     = new QWidget;
    auto* hlay     = new QHBoxLayout(cell);
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
            this, "Select Directory",
            pathEdit->text().isEmpty() ? QDir::homePath() : pathEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty())
            pathEdit->setText(dir);
    });

    hlay->addWidget(pathEdit, 1);
    hlay->addWidget(browseBtn, 0);
    table->setCellWidget(1, 1, cell);

    table->setRowHeight(0, 24);
    table->setRowHeight(1, 24);
    const int headerH = table->horizontalHeader()->sizeHint().height();
    table->setFixedHeight(headerH + 24 * 2 + 4);

    TableUtils::applyTableStyle(table, Page5Style::TABLE);
    return table;
}

// ══════════════════════════════════════════════════════
//  表格刷新（從 ViewModel 讀取資料）
// ══════════════════════════════════════════════════════
void Page5RightPanel::refreshInputTable()
{
    if (!m_inputTable || !m_viewModel) return;
    const auto& rows = m_viewModel->inputRows();

    m_inputTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        m_inputTable->setRowHeight(r, 22);

        // col 0：Seq
        m_inputTable->setItem(r, 0, TableUtils::makeCenteredItem(QString::number(r + 1)));

        // ★ col 1：Input（格式 Vin/Frequency/Phase，與 Page2 一致）
        const QString inputLabel = QString("%1/%2/%3")
                                       .arg(rows[r].vin)
                                       .arg(rows[r].frequency)
                                       .arg(rows[r].phase);
        m_inputTable->setItem(r, 1, TableUtils::makeCenteredItem(inputLabel));

        // col 2~4：Vin / Frequency / Phase
        m_inputTable->setItem(r, 2, TableUtils::makeCenteredItem(rows[r].vin));
        m_inputTable->setItem(r, 3, TableUtils::makeCenteredItem(rows[r].frequency));
        m_inputTable->setItem(r, 4, TableUtils::makeCenteredItem(rows[r].phase));
    }
    m_inputTable->resizeColumnsToContents();
    TableUtils::fitTableHeight(m_inputTable, 200);
}

void Page5RightPanel::refreshLoadTable()
{
    if (!m_loadTable || !m_viewModel) return;
    const auto& meta = m_viewModel->loadMeta();
    const auto& rows = m_viewModel->loadRows();

    const int N = static_cast<int>(meta.vo.size());
    if (N == 0) {
        m_loadTable->setRowCount(0);
        m_loadTable->setMaximumHeight(60);
        return;
    }

    const int colCount = N + 2;
    m_loadTable->setColumnCount(colCount);
    m_loadTable->clearContents();   // ★ 清除舊內容，防止 Power 欄殘留在舊位置
    QStringList headers{"Output"};
    for (int i = 1; i <= N; ++i) headers << QString("Index%1").arg(i);
    headers << "Power";
    m_loadTable->setHorizontalHeaderLabels(headers);
    m_loadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_loadTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_loadTable->horizontalHeader()->setSectionResizeMode(colCount - 1, QHeaderView::ResizeToContents);

    constexpr int META = 4;
    m_loadTable->setRowCount(META + rows.size());

    const QStringList metaLabels{"Mode", "Name", "Vo", "Von"};
    const QVector<const QVector<QString>*> metaVecs{
                                                     &meta.modes, &meta.names, &meta.vo, &meta.von};

    for (int mr = 0; mr < META; ++mr) {
        m_loadTable->setRowHeight(mr, 22);
        m_loadTable->setItem(mr, 0, TableUtils::makeMetaItem(metaLabels[mr]));
        for (int c = 0; c < N && c < metaVecs[mr]->size(); ++c)
            m_loadTable->setItem(mr, c + 1, TableUtils::makeMetaItem((*metaVecs[mr])[c]));
        m_loadTable->setItem(mr, colCount - 1, TableUtils::makeMetaItem(""));
    }

    for (int dr = 0; dr < rows.size(); ++dr) {
        const int tr = META + dr;
        m_loadTable->setRowHeight(tr, 22);
        m_loadTable->setItem(tr, 0, TableUtils::makeCenteredItem(rows[dr].label));
        double power    = 0.0;
        bool   hasPower = false;
        for (int c = 0; c < N && c < rows[dr].values.size(); ++c) {
            m_loadTable->setItem(tr, c + 1, TableUtils::makeCenteredItem(rows[dr].values[c]));
            if (c < meta.vo.size() && !meta.vo[c].isEmpty() && !rows[dr].values[c].isEmpty()) {
                power   += meta.vo[c].toDouble() * rows[dr].values[c].toDouble();
                hasPower = true;
            }
        }
        m_loadTable->setItem(tr, colCount - 1,
                             TableUtils::makeCenteredItem(hasPower ? QString::number(power, 'f', 3) : ""));
    }

    m_loadTable->resizeColumnsToContents();
    TableUtils::fitTableHeight(m_loadTable, 300);
}

void Page5RightPanel::refreshDynamicTable()
{
    if (!m_dynamicTable || !m_viewModel) return;
    const auto& meta = m_viewModel->dynamicMeta();
    const auto& rows = m_viewModel->dynamicRows();

    const int N = static_cast<int>(meta.vo.size());
    if (N == 0) {
        m_dynamicTable->setRowCount(0);
        m_dynamicTable->setMaximumHeight(60);
        return;
    }

    const int colCount = N + 2;
    m_dynamicTable->setColumnCount(colCount);
    m_dynamicTable->clearContents();   // ★ 清除舊內容，防止 T1~T2 欄殘留在舊位置
    QStringList headers{"Output"};
    for (int i = 1; i <= N; ++i) headers << QString("Index%1").arg(i);
    headers << "T1~T2(ms)";
    m_dynamicTable->setHorizontalHeaderLabels(headers);
    m_dynamicTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dynamicTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_dynamicTable->horizontalHeader()->setSectionResizeMode(colCount - 1, QHeaderView::ResizeToContents);

    constexpr int META = 2;
    m_dynamicTable->setRowCount(META + rows.size());

    const QStringList metaLabels{"Vo", "Von"};
    const QVector<const QVector<QString>*> metaVecs{&meta.vo, &meta.von};

    for (int mr = 0; mr < META; ++mr) {
        m_dynamicTable->setRowHeight(mr, 22);
        m_dynamicTable->setItem(mr, 0, TableUtils::makeMetaItem(metaLabels[mr]));
        for (int c = 0; c < N && c < metaVecs[mr]->size(); ++c)
            m_dynamicTable->setItem(mr, c + 1, TableUtils::makeMetaItem((*metaVecs[mr])[c]));
        m_dynamicTable->setItem(mr, colCount - 1, TableUtils::makeMetaItem(""));
    }

    for (int dr = 0; dr < rows.size(); ++dr) {
        const int tr = META + dr;
        m_dynamicTable->setRowHeight(tr, 22);
        m_dynamicTable->setItem(tr, 0, TableUtils::makeCenteredItem(rows[dr].label));
        for (int c = 0; c < N && c < rows[dr].values.size(); ++c)
            m_dynamicTable->setItem(tr, c + 1, TableUtils::makeCenteredItem(rows[dr].values[c]));
        const QString t1t2 = (dr < meta.t1t2.size()) ? meta.t1t2[dr] : QString();
        m_dynamicTable->setItem(tr, colCount - 1, TableUtils::makeCenteredItem(t1t2));
    }

    m_dynamicTable->resizeColumnsToContents();
    TableUtils::fitTableHeight(m_dynamicTable, 300);
}

void Page5RightPanel::refreshRelayTable()
{
    if (!m_relayTable || !m_viewModel) return;
    const auto& rows = m_viewModel->relayRows();

    if (rows.isEmpty()) {
        m_relayTable->setRowCount(0);
        m_relayTable->setMaximumHeight(60);
        return;
    }

    const int N        = rows[0].values.size();
    const int colCount = N + 1;
    m_relayTable->setColumnCount(colCount);
    m_relayTable->clearContents();   // ★ 清除舊內容，防止欄位殘留
    QStringList headers{"Relay"};
    for (int i = 1; i <= N; ++i) headers << QString("Index%1").arg(i);
    m_relayTable->setHorizontalHeaderLabels(headers);
    m_relayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_relayTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_relayTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        m_relayTable->setRowHeight(r, 22);
        m_relayTable->setItem(r, 0, TableUtils::makeCenteredItem(rows[r].label));
        for (int c = 0; c < N && c < rows[r].values.size(); ++c)
            m_relayTable->setItem(r, c + 1, TableUtils::makeCenteredItem(rows[r].values[c]));
    }

    m_relayTable->resizeColumnsToContents();
    TableUtils::fitTableHeight(m_relayTable, 200);
}
