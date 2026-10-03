#pragma once
#include "groupedconditionsmodel.h"
#include "styleutils.h"
#include "../../ui/table/conditionselection.h"
#include <QComboBox>
#include <QDoubleValidator>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include "../../ui/table/rowactionmenu.h"
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>

class GroupedConditionEditor : public QStyledItemDelegate
{
  public:
    explicit GroupedConditionEditor(QTableView *view) : QStyledItemDelegate(view), m_view(view) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        QWidget *widget;
        const auto choices = index.data(GroupedConditionsModel::ChoicesRole).toStringList();
        auto commit = [this](QWidget *editor) {
            emit const_cast<GroupedConditionEditor *>(this)->commitData(editor);
        };
        if (!choices.isEmpty()) {
            auto *editor = new QComboBox(parent);
            editor->addItems(choices);
            StyleUtils::applyComboBoxStyle(editor, true);
            connect(editor, &QComboBox::activated, this, [commit, editor] { commit(editor); });
            widget = editor;
        } else {
            auto *editor = new QLineEdit(parent);
            editor->setAlignment(Qt::AlignCenter);
            StyleUtils::applyLineEditStyle(editor);
            const auto type = index.data(GroupedConditionsModel::EditorRole).toString();
            if (type == "number") {
                auto *v = new QDoubleValidator(editor);
                v->setDecimals(3);
                v->setNotation(QDoubleValidator::StandardNotation);
                editor->setValidator(v);
            }
            if (type == "range" || type == "load")
                editor->setValidator(new QRegularExpressionValidator(
                    ConditionValues::editorPattern(type), editor));
            connect(editor, &QLineEdit::textChanged, this, [commit, editor] {
                if (editor->text().isEmpty() || editor->hasAcceptableInput())
                    commit(editor);
            });
            widget = editor;
        }
        widget->setProperty("conditionRow", index.row());
        widget->setProperty("conditionColumn", index.column());
        widget->installEventFilter(const_cast<GroupedConditionEditor *>(this));
        ConditionSelection::decorate(m_view, widget, index);
        return widget;
    }
    void setEditorData(QWidget *widget, const QModelIndex &index) const override
    {
        const QSignalBlocker blocker(widget);
        if (auto *combo = qobject_cast<QComboBox *>(widget)) {
            combo->clear();
            combo->addItems(index.data(GroupedConditionsModel::ChoicesRole).toStringList());
            combo->setCurrentText(index.data(Qt::EditRole).toString());
        } else
            QStyledItemDelegate::setEditorData(widget, index);
    }
    void setModelData(QWidget *widget, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(widget))
            model->setData(index, combo->currentText());
        else
            QStyledItemDelegate::setModelData(widget, model, index);
    }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress && qobject_cast<QLineEdit *>(object)) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            int dr = key == Qt::Key_Down ? 1 : key == Qt::Key_Up ? -1 : 0;
            int dc = key == Qt::Key_Right ? 1 : key == Qt::Key_Left ? -1 : 0;
            if (dr || dc) {
                int row = object->property("conditionRow").toInt(),
                    col = object->property("conditionColumn").toInt();
                for (row += dr, col += dc; row >= 0 && row < m_view->model()->rowCount() && col >= 0 &&
                                           col < m_view->model()->columnCount();
                     row += dr, col += dc) {
                    auto index = m_view->model()->index(row, col);
                    if (!m_view->isColumnHidden(col) && (index.flags() & Qt::ItemIsEditable)) {
                        if (auto *editor = m_view->indexWidget(index)) {
                            editor->setFocus();
                            return true;
                        }
                    }
                }
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }

  private:
    QTableView *m_view;
};

class GroupedConditionsView : public QTableView
{
  public:
    GroupedConditionsView(Page2ViewModel *vm, TableKind kind, QWidget *parent = nullptr)
        : QTableView(parent), m_rows(new GroupedConditionsModel(vm, kind, this)), m_vm(vm)
    {
        setObjectName(kind == TableKind::Dc     ? "dcSourceTable"
                      : kind == TableKind::Load ? "loadConditionTable"
                                                : "dynamicConditionTable");
        setModel(m_rows);
        setItemDelegate(new GroupedConditionEditor(this));
        StyleUtils::applyTableStyle(this);
        ConditionSelection::install(this);
        setSelectionBehavior(SelectRows);
        setSelectionMode(ExtendedSelection);
        verticalHeader()->hide();
        horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        horizontalHeader()->setDefaultSectionSize(qMax(100, fontMetrics().horizontalAdvance("Auto Range") + 24));
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        setColumnWidth(0, kind == TableKind::Dc ? 30 : 36);
        if (kind == TableKind::Dc) {
            horizontalHeader()->setDefaultSectionSize(50);
            setColumnWidth(1, 76);
            setColumnWidth(2, 84);
        }
        connect(m_rows, &QAbstractItemModel::modelReset, this,
                [this] { QTimer::singleShot(0, this, [this] { layoutEditors(); }); });
        connect(vm, &Page2ViewModel::dataChanged, this, [this] { updateVisibility(); });
        auto shortcut = [&](QKeySequence key, auto action) {
            connect(new QShortcut(key, this, nullptr, nullptr, Qt::WidgetShortcut), &QShortcut::activated,
                    this, action);
        };
        shortcut(QKeySequence::Copy, [this] { copyRows(); });
        shortcut(QKeySequence::Paste, [this] { pasteRows(); });
        shortcut(QKeySequence(Qt::Key_Delete), [this] { removeSelectedRows(); });
        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
            QMenu menu(this);
            const auto actions = RowActionMenu::populate(menu, selectedGroups().size(), m_clipboard.size());
            connect(actions.copy, &QAction::triggered, this, [this] { copyRows(); });
            connect(actions.paste, &QAction::triggered, this, [this] { pasteRows(); });
            connect(actions.remove, &QAction::triggered, this, [this] { removeSelectedRows(); });
            menu.exec(viewport()->mapToGlobal(position));
        });
        layoutEditors();
    }
    void appendRow()
    {
        m_rows->append();
        layoutEditors();
    }
    void refreshRows()
    {
        m_rows->refreshFromSource();
        layoutEditors();
    }
    void removeSelectedRows()
    {
        auto groups = selectedGroups();
        if (groups.isEmpty() && !selectionModel()->selectedIndexes().isEmpty())
            return;
        if (groups.isEmpty() && m_rows->recordCount())
            groups << m_rows->recordCount() - 1;
        if (!groups.isEmpty())
            m_rows->removeRecords(groups);
        layoutEditors();
    }

  protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            removeSelectedRows();
            return;
        }
        QTableView::keyPressEvent(event);
    }

  private:
    void updateVisibility()
    {
        if (m_rows->kind() == TableKind::Dc)
            for (int source = 0; source < 3; ++source)
                setColumnHidden(source + 3, source >= m_vm->dcInputs());
    }
    void layoutEditors()
    {
        clearSpans();
        if (m_rows->kind() == TableKind::Dc)
            for (int row = 0; row < m_rows->rowCount(); row += 2) {
                setSpan(row, 0, 2, 1);
                setSpan(row, 1, 2, 1);
            }
        updateVisibility();
        for (int row = 0; row < m_rows->rowCount(); ++row)
            for (int col = 1; col < m_rows->columnCount(); ++col) {
                const auto index = m_rows->index(row, col);
                if (index.flags() & Qt::ItemIsEditable)
                    ConditionSelection::openPersistentEditor(this, index);
            }
    }
    QList<int> selectedGroups() const
    {
        QList<int> groups;
        for (const auto &index : selectionModel()->selectedIndexes()) {
            const int group = m_rows->recordAt(index.row());
            if (group >= 0 && !groups.contains(group))
                groups << group;
        }
        std::sort(groups.begin(), groups.end());
        return groups;
    }
    void copyRows()
    {
        m_clipboard.clear();
        for (int group : selectedGroups())
            m_clipboard << m_rows->record(group);
    }
    void pasteRows()
    {
        const auto groups = selectedGroups();
        m_rows->insertRecords(groups.isEmpty() ? 0 : groups.last() + 1, m_clipboard);
        layoutEditors();
    }
    GroupedConditionsModel *m_rows;
    Page2ViewModel *m_vm;
    QVector<QStringList> m_clipboard;
};
