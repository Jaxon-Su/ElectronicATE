#include "dialogs/taskdialogeditor.h"
#include "page5centerpanel.h"
#include "page5viewmodel.h"
#include "tableutils.h"
#include "page5style.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QFrame>
#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include "../../ui/table/rowactionmenu.h"
#include <QAction>
#include <QShortcut>
#include <QKeyEvent>
#include <QTabWidget>
#include <QHeaderView>
#include <algorithm>
#include <QSignalBlocker>

// 讀取 cellWidget 內 QPushButton checked 狀態
static bool checkboxState(QTableWidget *t, int row, int col)
{
    auto *w = t->cellWidget(row, col);
    if (!w)
        return false;
    auto *btn = w->findChild<QPushButton *>();
    return btn ? btn->isChecked() : false;
}

Page5CenterPanel::Page5CenterPanel(Page5ViewModel *viewModel, QWidget *parent)
    : QWidget(parent), m_viewModel(viewModel)
{
    buildUi();
}

void Page5CenterPanel::setTaskList(const QStringList &tasks) { m_taskList = tasks; }

// 收集表格所有列 → 發射信號給 ViewModel
void Page5CenterPanel::notifyDutRowsChanged()
{
    clearResults();
    QVector<DutRowData> rows;
    rows.reserve(m_dutTestTable->rowCount());
    for (int r = 0; r < m_dutTestTable->rowCount(); ++r) {
        DutRowData d;
        d.uid = uidOfRow(r);
        d.active = checkboxState(m_dutTestTable, r, 1);
        d.item = m_dutTestTable->item(r, 2) ? m_dutTestTable->item(r, 2)->text() : "";
        d.ext = m_dutTestTable->item(r, 3) ? m_dutTestTable->item(r, 3)->text() : "";
        d.retry = m_dutTestTable->item(r, 4) ? m_dutTestTable->item(r, 4)->text() : "0";
        d.report = checkboxState(m_dutTestTable, r, 5);
        d.settings = collectSettingsForUid(uidOfRow(r)); // ★ Dialog 設定一起收集
        rows.append(d);
    }
    emit dutRowsChanged(rows);
    syncActiveTasksToRunPanel(); // 任何列變動都同步至 RunPanel
}

// XML 載入後，從 ViewModel 讀取資料填表
void Page5CenterPanel::loadDutRows()
{
    if (!m_viewModel)
        return;
    const auto rows = m_viewModel->dutRows();

    // 設旗標：程式碼填表期間靜默，不觸發 itemChanged / toggled
    m_loadingData = true;
    m_dutTestTable->setRowCount(0);
    for (const auto &d : rows)
        insertDutRow(m_dutTestTable->rowCount(), d);
    refreshDutSeq();
    m_loadingData = false; // ★ 恢復正常觸發
    syncActiveTasksToRunPanel();
}

void Page5CenterPanel::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *vSplitter = new QSplitter(Qt::Vertical, this);
    vSplitter->addWidget(buildTabWidget());
    vSplitter->addWidget(buildOutputSection());
    vSplitter->setStretchFactor(0, 3);
    vSplitter->setStretchFactor(1, 1);
    vSplitter->setSizes({500, 130});
    vSplitter->setChildrenCollapsible(false);
    vSplitter->setStyleSheet(Page5Style::SPLITTER);

    layout->addWidget(vSplitter);
}

QWidget *Page5CenterPanel::buildTabWidget()
{
    m_tabs = new QTabWidget;
    m_tabs->setStyleSheet(Page5Style::TAB);

    // Tab 0: Tasks → RunPanel
    m_runPanel = new Page5RunPanel;
    m_tabs->addTab(m_runPanel, "Tasks");

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // 標題列
    auto *titleBar = new QWidget;
    auto *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel("DUT Test");
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    titleBarLayout->addWidget(title);
    titleBarLayout->addStretch();
    layout->addWidget(titleBar);

    m_dutTestTable = buildDUTTestTable();
    layout->addWidget(m_dutTestTable);
    m_dutTestTable->installEventFilter(this);

    // 雙擊列 → 開啟子視窗
    connect(m_dutTestTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col == 3 || col == 4 || col == 6)
            return;

        const QString testItem =
            m_dutTestTable->item(row, 2) ? m_dutTestTable->item(row, 2)->text() : "Unknown";
        const QString extName = m_dutTestTable->item(row, 3) ? m_dutTestTable->item(row, 3)->text() : "";

        // 取得穩定 uid（增刪列後仍正確對應設定）
        const int uid = uidOfRow(row);

        if (m_viewModel && TaskDialogEditor(*m_viewModel, uid, row + 1, extName, this).edit(testItem))
            notifyDutRowsChanged();
    });

    m_tabs->addTab(page, "DUT Test");
    m_tabs->setCurrentIndex(1);
    return m_tabs;
}

QTableWidget *Page5CenterPanel::buildDUTTestTable()
{
    auto *table = new QTableWidget(0, 6);
    table->setHorizontalHeaderLabels({"Seq", "Active", "Test Item", "Ext. Name", "Fail Retry", "Report"});

    auto *hdr = table->horizontalHeader();
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);
    hdr->setSectionResizeMode(3, QHeaderView::Stretch);
    hdr->resizeSection(0, 40);
    hdr->resizeSection(1, 55);
    hdr->resizeSection(4, 72);
    hdr->resizeSection(5, 60);

    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    TableUtils::applyTableStyle(table, Page5Style::TABLE);

    // Ctrl+C
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, table, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &Page5CenterPanel::copySelectedRows);

    // Ctrl+V
    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, table, nullptr, nullptr, Qt::WidgetShortcut);
    connect(pasteShortcut, &QShortcut::activated, this, [this]() {
        const auto idxList = m_dutTestTable->selectionModel()->selectedRows();
        int insertAfter = -1;
        for (const auto &idx : idxList)
            insertAfter = qMax(insertAfter, idx.row());
        pasteRows(insertAfter);
    });

    // 右鍵選單
    connect(table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const auto idxList = m_dutTestTable->selectionModel()->selectedRows();
        QList<int> selRows;
        for (const auto &idx : idxList)
            selRows.append(idx.row());
        std::sort(selRows.begin(), selRows.end());

        const int clickedRow = m_dutTestTable->rowAt(pos.y());
        const bool hasSelection = !selRows.isEmpty();
        const bool hasClipboard = !m_clipboard.isEmpty();

        QMenu menu(this);
        const QString menuStyle = RowActionMenu::style();
        menu.setStyleSheet(menuStyle);

        // 新增子選單（使用 m_taskList）
        QMenu *addMenu = menu.addMenu("➕  Add Row");
        addMenu->setStyleSheet(menuStyle);
        for (int i = 0; i < m_taskList.size(); ++i) {
            const QString name = m_taskList[i];
            if (name.isEmpty())
                continue;
            QAction *act = addMenu->addAction(QString("%1.  %2").arg(i + 1).arg(name));
            connect(act, &QAction::triggered, this, [this, name]() { addDutTestRow(name); });
        }

        menu.addSeparator();

        // 複製
        const QString copyLabel = hasSelection ? QString("📋  Copy  (%1 row%2)  Ctrl+C")
                                                     .arg(selRows.size())
                                                     .arg(selRows.size() > 1 ? "s" : "")
                                               : "📋  Copy  Ctrl+C";
        QAction *copyAct = menu.addAction(copyLabel);
        copyAct->setEnabled(hasSelection);
        connect(copyAct, &QAction::triggered, this, &Page5CenterPanel::copySelectedRows);

        // 貼上
        const QString pasteLabel = hasClipboard ? QString("📌  Paste  (%1 row%2 in clipboard)  Ctrl+V")
                                                      .arg(m_clipboard.size())
                                                      .arg(m_clipboard.size() > 1 ? "s" : "")
                                                : "📌  Paste  (clipboard empty)  Ctrl+V";
        QAction *pasteAct = menu.addAction(pasteLabel);
        pasteAct->setEnabled(hasClipboard);
        connect(pasteAct, &QAction::triggered, this, [this, clickedRow]() { pasteRows(clickedRow); });

        menu.addSeparator();

        // 刪除
        const QString delLabel =
            hasSelection
                ? QString("🗑  Delete  (%1 row%2)").arg(selRows.size()).arg(selRows.size() > 1 ? "s" : "")
                : "🗑  Delete";
        QAction *delAct = menu.addAction(delLabel);
        delAct->setEnabled(hasSelection);
        connect(delAct, &QAction::triggered, this, [this, selRows]() {
            for (int i = selRows.size() - 1; i >= 0; --i) {
                removeSettingsForUid(uidOfRow(selRows[i]));
                m_dutTestTable->removeRow(selRows[i]);
            }
            m_dutTestTable->clearSelection();
            refreshDutSeq();
            notifyDutRowsChanged();
        });

        menu.exec(m_dutTestTable->viewport()->mapToGlobal(pos));
    });

    // Ext.Name（col 3）/ Fail Retry（col 4）直接編輯 → 存入 Model
    //   m_loadingData guard：程式碼填表時不觸發（避免無限迴圈）
    connect(table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_loadingData)
            return;
        const int col = item->column();
        if (col == 3 || col == 4) // Ext.Name or Fail Retry
            notifyDutRowsChanged();
    });

    return table;
}

QWidget *Page5CenterPanel::buildOutputSection()
{
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *bar = new QWidget;
    bar->setStyleSheet("background-color: #dce6f1;");
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(6, 3, 6, 3);
    auto *lbl = new QLabel("Output Window");
    lbl->setStyleSheet("font-weight: bold; color: #333;");
    barLayout->addWidget(lbl);
    barLayout->addStretch();
    layout->addWidget(bar);

    m_outputWindow = new QTextEdit;
    m_outputWindow->setReadOnly(true);
    m_outputWindow->setStyleSheet(Page5Style::OUTPUT_WINDOW);
    m_outputWindow->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_outputWindow, &QTextEdit::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(m_outputWindow);
        menu.setStyleSheet(R"(
                QMenu {
                    background: #ffffff;
                    border: 1px solid #c8d0e0;
                    border-radius: 4px;
                    padding: 4px 0px;
                    font-size: 12px;
                }
                QMenu::item { padding: 6px 32px 6px 16px; color: #2c3e50; }
                QMenu::item:selected  { background: #e8f0fe; color: #1a56db; }
                QMenu::item:disabled  { color: #bbb; }
                QMenu::separator { height: 1px; background: #e0e5ef; margin: 3px 8px; }
            )");

        QAction *clearAct = menu.addAction("🗑  Clear Output");
        clearAct->setEnabled(!m_outputWindow->document()->isEmpty());
        connect(clearAct, &QAction::triggered, this, [this]() { m_outputWindow->clear(); });

        menu.exec(m_outputWindow->viewport()->mapToGlobal(pos));
    });

    layout->addWidget(m_outputWindow);
    return container;
}

//  DUT Test 動態列管理
void Page5CenterPanel::addDutTestRow(const QString &taskName)
{
    DutRowData d;
    d.item = taskName;
    d.active = true; // 新增時預設勾選 Active
    const int at = m_dutTestTable->rowCount();
    insertDutRow(at, d);

    refreshDutSeq();
    m_dutTestTable->scrollToBottom();
    notifyDutRowsChanged(); // 內部已含 syncActiveTasksToRunPanel
}

void Page5CenterPanel::refreshDutSeq()
{
    for (int r = 0; r < m_dutTestTable->rowCount(); ++r) {
        if (auto *item = m_dutTestTable->item(r, 0))
            item->setText(QString::number(r + 1));
    }
}

void Page5CenterPanel::copySelectedRows()
{
    const auto idxList = m_dutTestTable->selectionModel()->selectedRows();
    if (idxList.isEmpty())
        return;

    QList<int> rows;
    for (const auto &idx : idxList)
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end());

    m_clipboard.clear();
    for (int r : rows) {
        DutRowData d;
        d.uid = uidOfRow(r);
        d.active = checkboxState(m_dutTestTable, r, 1);
        d.item = m_dutTestTable->item(r, 2) ? m_dutTestTable->item(r, 2)->text() : "";
        d.ext = m_dutTestTable->item(r, 3) ? m_dutTestTable->item(r, 3)->text() : "";
        d.retry = m_dutTestTable->item(r, 4) ? m_dutTestTable->item(r, 4)->text() : "0";
        d.report = checkboxState(m_dutTestTable, r, 5);
        d.settings = collectSettingsForUid(uidOfRow(r));
        m_clipboard.append(d);
    }
}

void Page5CenterPanel::pasteRows(int insertAfterRow)
{
    if (m_clipboard.isEmpty())
        return;

    int insertAt = (insertAfterRow >= 0) ? insertAfterRow + 1 : m_dutTestTable->rowCount();

    m_dutTestTable->clearSelection();
    for (const auto &d : std::as_const(m_clipboard)) {
        insertDutRow(insertAt, d);
        m_dutTestTable->selectRow(insertAt);
        ++insertAt;
    }

    refreshDutSeq();
    m_dutTestTable->scrollTo(m_dutTestTable->model()->index(insertAt - 1, 0));
    notifyDutRowsChanged(); // ← 同步到 ViewModel
}

void Page5CenterPanel::insertDutRow(int at, const DutRowData &d)
{
    const QSignalBlocker blockTable(m_dutTestTable);
    m_dutTestTable->insertRow(at);

    // 分配穩定 UID，增刪其他列後此值不變
    const int uid = m_loadingData && d.uid >= 0 ? d.uid : m_viewModel->appendTask(d);
    auto *seqItem = TableUtils::makeCenteredItem("0");
    seqItem->setFlags(seqItem->flags() & ~Qt::ItemIsEditable);
    seqItem->setData(Qt::UserRole, uid);
    m_dutTestTable->setItem(at, 0, seqItem);

    m_dutTestTable->setCellWidget(at, 1, TableUtils::makeCenteredCheckbox(d.active));

    // Active 勾選變更 → 存入 Model（notifyDutRowsChanged 內含 syncActiveTasksToRunPanel）
    if (auto *w = m_dutTestTable->cellWidget(at, 1)) {
        if (auto *btn = w->findChild<QPushButton *>()) {
            connect(btn, &QPushButton::toggled, this, [this](bool) {
                if (!m_loadingData)
                    notifyDutRowsChanged();
            });
        }
    }

    auto *itemCell = new QTableWidgetItem(d.item);
    itemCell->setFlags(itemCell->flags() & ~Qt::ItemIsEditable);
    m_dutTestTable->setItem(at, 2, itemCell);

    m_dutTestTable->setItem(at, 3, new QTableWidgetItem(d.ext));

    m_dutTestTable->setItem(at, 4, TableUtils::makeCenteredItem(d.retry));
    m_dutTestTable->setCellWidget(at, 5, TableUtils::makeCenteredCheckbox(d.report));
    // Report 勾選變更 → 存入 Model
    if (auto *w = m_dutTestTable->cellWidget(at, 5)) {
        if (auto *btn = w->findChild<QPushButton *>()) {
            connect(btn, &QPushButton::toggled, this, [this](bool) {
                if (!m_loadingData)
                    notifyDutRowsChanged();
            });
        }
    }

    m_dutTestTable->setRowHeight(at, 26);
}

bool Page5CenterPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_dutTestTable && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            const auto idxList = m_dutTestTable->selectionModel()->selectedRows();
            if (idxList.isEmpty())
                return false;

            QList<int> rows;
            for (const auto &idx : idxList)
                rows.append(idx.row());
            std::sort(rows.begin(), rows.end(), std::greater<int>());
            for (int r : rows) {
                removeSettingsForUid(uidOfRow(r));
                m_dutTestTable->removeRow(r);
            }
            m_dutTestTable->clearSelection();
            refreshDutSeq();
            notifyDutRowsChanged();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// syncActiveTasksToRunPanel
// 收集 RunTask（uid 穩定，增刪列後不錯位）
void Page5CenterPanel::syncActiveTasksToRunPanel()
{
    if (!m_runPanel)
        return;

    QVector<RunTask> activeTasks;
    for (int r = 0; r < m_dutTestTable->rowCount(); ++r) {
        if (!checkboxState(m_dutTestTable, r, 1))
            continue;
        const QString name = m_dutTestTable->item(r, 2) ? m_dutTestTable->item(r, 2)->text() : QString();
        if (name.isEmpty())
            continue;
        activeTasks.append({uidOfRow(r), name});
    }
    m_runPanel->setActiveTasks(activeTasks);
}

// uidOfRow
int Page5CenterPanel::uidOfRow(int row) const
{
    auto *item = m_dutTestTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

// removeSettingsForUid
void Page5CenterPanel::removeSettingsForUid(int uid) { m_viewModel->removeTask(uid); }

// appendLog
void Page5CenterPanel::appendLog(const QString &msg)
{
    if (m_outputWindow)
        m_outputWindow->append(msg);
}

QVariantMap Page5CenterPanel::collectSettingsForUid(int uid) const { return m_viewModel->taskSettings(uid); }

// setLocked：執行中鎖定整個編輯區，只保留 RunPanel Stop 可用
void Page5CenterPanel::setLocked(bool locked)
{
    // ① 停用 Tab 切換（tab bar 本身）
    m_tabs->tabBar()->setEnabled(!locked);

    // ② DUT Test 頁（index 1）整頁停用：表格、checkbox 全部不可互動
    if (m_tabs->count() > 1) {
        QWidget *dutPage = m_tabs->widget(1);
        if (dutPage)
            dutPage->setEnabled(!locked);
    }

    // ③ 清空 clipboard，防止解鎖後殘留貼上
    if (locked)
        m_clipboard.clear();
    // tab 切換決策已移至 page5.cpp::setupConnections，不在 View 自己判斷
}

// switchToRunTab：由 page5 協調層在 Run 時呼叫
void Page5CenterPanel::switchToRunTab()
{
    if (m_tabs)
        m_tabs->setCurrentIndex(0); // index 0 = Tasks（RunPanel）
}

void Page5CenterPanel::clearResults() { m_runPanel->resetTaskRows(); }

void Page5CenterPanel::setTaskResult(int runIndex, const QString &result)
{
    m_runPanel->setTaskResult(runIndex, result);
}
