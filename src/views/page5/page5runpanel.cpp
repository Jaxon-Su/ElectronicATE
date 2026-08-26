#include "page5runpanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QFont>

QString Page5RunPanel::statusText(TaskStatus s) const
{
    switch (s) {
    case TaskStatus::Idle:    return "—";
    case TaskStatus::Running: return "▶  Running";
    case TaskStatus::Pass:    return "✔  Pass";
    case TaskStatus::Fail:    return "✘  Fail";
    }
    return {};
}

QString Page5RunPanel::statusStyle(TaskStatus s) const
{
    switch (s) {
    case TaskStatus::Idle:    return "color: #9AAAC8; font-weight: bold;";
    case TaskStatus::Running: return "color: #2A5298; font-weight: bold;";
    case TaskStatus::Pass:    return "color: #27AE60; font-weight: bold;";
    case TaskStatus::Fail:    return "color: #E74C3C; font-weight: bold;";
    }
    return {};
}

Page5RunPanel::Page5RunPanel(QWidget* parent) : QWidget(parent) { buildUi(); }

void Page5RunPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);
    buildToolBar(this, root);
    buildTaskTable(this, root);
}

void Page5RunPanel::buildToolBar(QWidget* parent, QVBoxLayout* layout)
{
    auto* bar = new QWidget(parent);
    bar->setStyleSheet("background: #EEF2FA; border-radius: 5px;");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(8);

    auto* title = new QLabel("Test Task", bar);
    title->setStyleSheet("font-size: 15px; font-weight: bold; color: #1A2A4A; background: transparent;");
    h->addWidget(title);
    h->addStretch();

    m_statusLbl = new QLabel("Idle", bar);
    m_statusLbl->setStyleSheet(
        "color: #9AAAC8; font-size: 11px; font-weight: bold;"
        "background: transparent; padding: 2px 8px;");
    h->addWidget(m_statusLbl);

    auto* sep = new QFrame(bar);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #C8D4E8; background: transparent;");
    h->addWidget(sep);

    m_runBtn = new QPushButton("▶  Run", bar);
    m_runBtn->setFixedSize(90, 30);
    m_runBtn->setStyleSheet(R"(
        QPushButton { background-color:#27AE60; color:white; border:none; border-radius:4px; font-size:11pt; font-weight:bold; }
        QPushButton:hover   { background-color:#2ECC71; }
        QPushButton:pressed { background-color:#1E8449; }
        QPushButton:disabled{ background-color:#A8D5B5; color:#FFFFFF; }
    )");

    m_stopBtn = new QPushButton("■  Stop", bar);
    m_stopBtn->setFixedSize(90, 30);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(R"(
        QPushButton { background-color:#E74C3C; color:white; border:none; border-radius:4px; font-size:11pt; font-weight:bold; }
        QPushButton:hover   { background-color:#FF6B6B; }
        QPushButton:pressed { background-color:#C0392B; }
        QPushButton:disabled{ background-color:#F1A9A0; color:#FFFFFF; }
    )");

    h->addWidget(m_runBtn);
    h->addWidget(m_stopBtn);
    layout->addWidget(bar);

    connect(m_runBtn, &QPushButton::clicked, this, [this]() {
        if (m_tasks.isEmpty()) return;
        m_running = true;
        updateControlState(true);
        m_statusLbl->setText("Running…");
        m_statusLbl->setStyleSheet(
            "color:#2A5298; font-size:11px; font-weight:bold; background:transparent; padding:2px 8px;");
        emit runRequested(m_tasks);   // QVector<RunTask>，含 dutUid
    });

    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        m_running = false;
        updateControlState(false);
        m_statusLbl->setText("Stopped");
        m_statusLbl->setStyleSheet(
            "color:#E67E22; font-size:11px; font-weight:bold; background:transparent; padding:2px 8px;");
        emit stopRequested();
    });
}

void Page5RunPanel::buildTaskTable(QWidget* parent, QVBoxLayout* layout)
{
    m_table = new QTableWidget(0, 4, parent);
    m_table->setHorizontalHeaderLabels({"Seq", "Task Item", "Retry", "Status"});

    auto* hdr = m_table->horizontalHeader();
    hdr->resizeSection(0, 45);
    hdr->setSectionResizeMode(1, QHeaderView::Stretch);
    hdr->resizeSection(2, 55);
    hdr->resizeSection(3, 120);
    hdr->setStyleSheet(R"(
        QHeaderView::section { background-color:#D6E2F5; color:#1A2A4A; font-weight:bold;
            font-size:10pt; border:none; border-bottom:2px solid #2A5298; padding:4px; }
    )");

    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(R"(
        QTableWidget { background-color:#FAFCFF; alternate-background-color:#F0F5FF;
            border:1px solid #C8D4E8; border-radius:4px; gridline-color:transparent; outline:none; }
        QTableWidget::item { height:30px; color:#2D2D2D; font-size:10pt; padding-left:6px; }
        QTableWidget::item:selected { background-color:#D6E4FF; color:#1A2A4A; }
    )");

    layout->addWidget(m_table);
}

// ★ 接收 QVector<RunTask>（含 dutUid）
void Page5RunPanel::setActiveTasks(const QVector<RunTask>& tasks)
{
    m_tasks = tasks;
    m_table->setRowCount(0);

    for (int i = 0; i < tasks.size(); ++i) {
        m_table->insertRow(i);

        auto* seqItem = new QTableWidgetItem(QString::number(i + 1));
        seqItem->setTextAlignment(Qt::AlignCenter);
        seqItem->setFlags(seqItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, seqItem);

        auto* nameItem = new QTableWidgetItem(tasks[i].name);
        nameItem->setFont([]{ QFont f; f.setPointSize(10); f.setBold(true); return f; }());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 1, nameItem);

        auto* retryItem = new QTableWidgetItem("0");
        retryItem->setTextAlignment(Qt::AlignCenter);
        retryItem->setFlags(retryItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 2, retryItem);

        auto* lbl = new QLabel(statusText(TaskStatus::Idle));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(statusStyle(TaskStatus::Idle));
        m_table->setCellWidget(i, 3, lbl);
        m_table->setRowHeight(i, 30);
    }
}

void Page5RunPanel::setRetryCount(int index, int count)
{
    if (index < 0 || index >= m_table->rowCount()) return;
    auto* item = m_table->item(index, 2);
    if (item)
        item->setText(QString::number(count));
}

void Page5RunPanel::setTaskStatus(int index, TaskStatus status)
{
    if (index < 0 || index >= m_table->rowCount()) return;
    auto* lbl = qobject_cast<QLabel*>(m_table->cellWidget(index, 3));
    if (lbl) {
        lbl->setText(statusText(status));
        lbl->setStyleSheet(statusStyle(status));
    }

    if (status == TaskStatus::Pass || status == TaskStatus::Fail) {
        bool allDone = true;
        for (int r = 0; r < m_table->rowCount(); ++r) {
            auto* l = qobject_cast<QLabel*>(m_table->cellWidget(r, 3));
            if (l && (l->text().contains("Running") || l->text() == "—"))
                allDone = false;
        }
        if (allDone) {
            m_running = false;
            updateControlState(false);
            m_statusLbl->setText("Done");
            m_statusLbl->setStyleSheet(
                "color:#27AE60; font-size:11px; font-weight:bold; background:transparent; padding:2px 8px;");
            emit stopRequested();
        }
    }
}

void Page5RunPanel::resetTaskRows()
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (auto* item = m_table->item(r, 2))
            item->setText("0");
        auto* lbl = qobject_cast<QLabel*>(m_table->cellWidget(r, 3));
        if (lbl) {
            lbl->setText(statusText(TaskStatus::Idle));
            lbl->setStyleSheet(statusStyle(TaskStatus::Idle));
        }
    }
}

void Page5RunPanel::resetAll()
{
    resetTaskRows();
    m_running = false;
    updateControlState(false);
    m_statusLbl->setText("Idle");
    m_statusLbl->setStyleSheet(
        "color:#9AAAC8; font-size:11px; font-weight:bold; background:transparent; padding:2px 8px;");
}

void Page5RunPanel::updateControlState(bool running)
{
    m_runBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);
}
