#include "page5leftpanel.h"
#include "page5treestyle.h"
#include "page5style.h"
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ── 靜態分群定義 ──────────────────────────────────────
const QList<Page5LeftPanel::TaskGroup> Page5LeftPanel::TASK_GROUPS = {
    {
        "Power Test",
        { "Static Test", "Dynamic Test",
         "Turn on then short", "Short then turn on",
         "Turn on", "Turn off" }
    },
    {
        "Instrument Control",
        { "Relay", "Write Oscilloscope" }
    },
    {
        "Time Control",
        { "Delay" }
    },
    };

// ── Panel Title ───────────────────────────────────────
static QLabel* makePanelTitle(const QString& text, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(Page5Style::PANEL_TITLE);
    return label;
}

// ── Constructor ───────────────────────────────────────
Page5LeftPanel::Page5LeftPanel(QWidget* parent)
    : SidePanel(Qt::LeftEdge, 220, parent)
{
    auto* cl = contentLayout();
    cl->addWidget(makePanelTitle("Test Group"));

    // ── ComboBox selector ──
    m_selector = new QComboBox(this);
    m_selector->addItem("Stress Test");
    m_selector->addItem("HP Test");
    m_selector->setStyleSheet(R"(
        QComboBox {
            background-color: #EEF2FA;
            color: #2D2D2D;
            border: 1px solid #C8D4E8;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 9pt;
            font-weight: bold;
        }
        QComboBox:hover {
            border-color: #2A5298;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            selection-background-color: #2A5298;
            selection-color: #FFFFFF;
            border: 1px solid #C8D4E8;
        }
    )");
    cl->addWidget(m_selector);

    // ── Stacked widget ──
    m_stack = new QStackedWidget(this);

    // Page 0 — Stress Test（現有 TASK_GROUPS）
    QList<QPair<QString, QStringList>> stressGroups;
    for (const auto& g : TASK_GROUPS)
        stressGroups.append({ g.groupName, g.tasks });
    m_taskTree = buildTree(stressGroups);
    m_stack->addWidget(m_taskTree);

    // Page 1 — DUT Test（placeholder，未來替換）
    QList<QPair<QString, QStringList>> dutGroups = {
        { "HP Item", { "brown out" } },   // ← 替換成實際內容
    };
    m_stack->addWidget(buildTree(dutGroups));

    cl->addWidget(m_stack);
    cl->addStretch();

    // ── 切換邏輯 ──
    connect(m_selector, &QComboBox::currentIndexChanged,
            m_stack,    &QStackedWidget::setCurrentIndex);
}

// ── buildTree ─────────────────────────────────────────
QTreeWidget* Page5LeftPanel::buildTree(
    const QList<QPair<QString, QStringList>>& groups)
{
    auto* tree = new QTreeWidget(this);
    tree->setStyle(new Page5TreeStyle());
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setIndentation(14);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setStyleSheet(R"(
        QTreeWidget {
            background-color: #F5F7FB;
            color: #2D2D2D;
            border: none;
            outline: none;
        }
        QTreeWidget::item {
            height: 28px;
            padding-left: 2px;
            color: #3D3D3D;
        }
        QTreeWidget::item:selected {
            background-color: #2A5298;
            color: #FFFFFF;
        }
        QTreeWidget::item:hover:!selected {
            background-color: #E8EEF7;
            color: #1A1A1A;
        }
        QTreeWidget::branch {
            background-color: #F5F7FB;
        }
    )");

    QFont groupFont;
    groupFont.setBold(true);
    groupFont.setPointSize(10);

    QFont taskFont;
    taskFont.setPointSize(9);
    taskFont.setBold(true);

    for (const auto& [groupName, tasks] : groups) {
        auto* groupItem = new QTreeWidgetItem(tree);
        groupItem->setText(0, groupName);
        groupItem->setFont(0, groupFont);
        groupItem->setForeground(0, QColor("#1A5FAD"));
        groupItem->setFlags(Qt::ItemIsEnabled);
        groupItem->setExpanded(true);

        for (const QString& taskName : tasks) {
            auto* taskItem = new QTreeWidgetItem(groupItem);
            taskItem->setText(0, taskName);
            taskItem->setFont(0, taskFont);
            taskItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
    }

    connect(tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
                if (item && item->parent() && !m_locked)
                    emit taskDoubleClicked(item->text(0).trimmed());
            });

    // ── viewport event filter：修正首次雙擊需點兩次的問題 ──
    tree->viewport()->installEventFilter(this);

    return tree;
}

// ── eventFilter：攔截 viewport 雙擊，繞過 Qt 選中延遲問題 ──
bool Page5LeftPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        // 找出是哪一棵 tree 的 viewport
        for (int i = 0; i < m_stack->count(); ++i) {
            auto* tree = qobject_cast<QTreeWidget*>(m_stack->widget(i));
            if (!tree || tree->viewport() != watched) continue;

            auto* me   = static_cast<QMouseEvent*>(event);
            auto* item = tree->itemAt(me->pos());
            if (item && item->parent() && !m_locked) {   // locked 時不觸發
                emit taskDoubleClicked(item->text(0).trimmed());
                return true;                            // 吃掉事件，不讓 Qt 再處理
            }
        }
    }
    return SidePanel::eventFilter(watched, event);
}

// ── setLocked：執行中停用雙擊加入任務 ────────────────
void Page5LeftPanel::setLocked(bool locked)
{
    m_locked = locked;
    // ComboBox 切換也停用，避免執行中切換 tree
    m_selector->setEnabled(!locked);
}

// ── taskNames ─────────────────────────────────────────
QStringList Page5LeftPanel::taskNames() const
{
    QStringList names;
    for (int g = 0; g < m_taskTree->topLevelItemCount(); ++g) {
        auto* groupItem = m_taskTree->topLevelItem(g);
        for (int t = 0; t < groupItem->childCount(); ++t)
            names << groupItem->child(t)->text(0).trimmed();
    }
    return names;
}
