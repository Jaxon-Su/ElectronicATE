#include "conditionrowsview.h"
#include "styleutils.h"
#include "../../ui/table/conditionselection.h"
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <QPersistentModelIndex>
#include <QHeaderView>
#include <QMenu>
#include "../../ui/table/rowactionmenu.h"
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>

namespace
{
class ConditionEditor : public QStyledItemDelegate
{
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        const auto choices = index.data(ConditionRowsModel::ChoicesRole).toStringList();
        if (!choices.isEmpty()) {
            auto *editor = new QComboBox(parent);
            editor->addItems(choices);
            StyleUtils::applyComboBoxStyle(editor, true);
            connect(editor, &QComboBox::activated, this,
                    [this, editor] { emit const_cast<ConditionEditor *>(this)->commitData(editor); });
            ConditionSelection::decorate(qobject_cast<QTableView *>(this->parent()), editor, index);
            return editor;
        }
        auto *editor = new QLineEdit(parent);
        editor->setAlignment(Qt::AlignCenter);
        StyleUtils::applyLineEditStyle(editor);
        if (index.column() >= 3) {
            auto *validator = new QDoubleValidator(editor);
            validator->setDecimals(3);
            validator->setNotation(QDoubleValidator::StandardNotation);
            editor->setValidator(validator);
        }
        editor->setProperty("conditionIndex", QVariant::fromValue(QPersistentModelIndex(index)));
        editor->installEventFilter(const_cast<ConditionEditor *>(this));
        ConditionSelection::decorate(qobject_cast<QTableView *>(this->parent()), editor, index);
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        const QSignalBlocker blocker(editor);
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            combo->setCurrentText(index.data(Qt::EditRole).toString());
        else
            QStyledItemDelegate::setEditorData(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            model->setData(index, combo->currentText());
        else
            QStyledItemDelegate::setModelData(editor, model, index);
    }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto *editor = qobject_cast<QLineEdit *>(object);
        auto *view = qobject_cast<QTableView *>(parent());
        if (editor && view && event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            const int dr = key->key() == Qt::Key_Down ? 1 : key->key() == Qt::Key_Up ? -1 : 0;
            const int dc = key->key() == Qt::Key_Right ? 1 : key->key() == Qt::Key_Left ? -1 : 0;
            const auto current = editor->property("conditionIndex").value<QPersistentModelIndex>();
            if (key->modifiers() == Qt::NoModifier && (dr || dc) && current.isValid()) {
                if (!editor->text().isEmpty() && !editor->hasAcceptableInput())
                    return true;
                for (int row = current.row() + dr, column = current.column() + dc;
                     row >= 0 && row < view->model()->rowCount() && column >= 0 &&
                     column < view->model()->columnCount(); row += dr, column += dc) {
                    const QPersistentModelIndex next(view->model()->index(row, column));
                    if (view->isRowHidden(row) || view->isColumnHidden(column) ||
                        !(next.flags() & Qt::ItemIsEditable) || !view->indexWidget(next))
                        continue;
                    emit commitData(editor);
                    if (next.isValid()) {
                        view->setCurrentIndex(next);
                        view->scrollTo(next);
                        if (auto *target = view->indexWidget(next))
                            target->setFocus(Qt::OtherFocusReason);
                    }
                    return true;
                }
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }
};
} // namespace
ConditionRowsView::ConditionRowsView(Page2ViewModel *vm, TableKind kind, QWidget *parent)
    : QTableView(parent), m_rows(new ConditionRowsModel(vm, kind, this))
{
    setObjectName(kind == TableKind::Input ? "acConditionTable" : "relayConditionTable");
    setModel(m_rows);
    setItemDelegate(new ConditionEditor(this));
    StyleUtils::applyTableStyle(this);
    ConditionSelection::install(this);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::AnyKeyPressed);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setDefaultSectionSize(64);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    setColumnWidth(0, 30);
    if (kind == TableKind::Input) {
        const QList<int> widths{30, 78, 56, 50, 72, 50};
        for (int column = 1; column < widths.size(); ++column)
            setColumnWidth(column, widths[column]);
    } else {
        setColumnWidth(1, 96);
    }
    verticalHeader()->hide();
    const auto openEditors = [this] {
        for (int row = 0; row < m_rows->rowCount(); ++row)
            for (int column = m_rows->firstEditableColumn(); column < m_rows->columnCount(); ++column)
                ConditionSelection::openPersistentEditor(this, m_rows->index(row, column));
    };
    connect(m_rows, &QAbstractItemModel::modelReset, this,
            [this, openEditors] { QTimer::singleShot(0, this, openEditors); });
    openEditors();
    connect(new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut), &QShortcut::activated, this,
            &ConditionRowsView::copyRows);
    connect(new QShortcut(QKeySequence::Paste, this, nullptr, nullptr, Qt::WidgetShortcut), &QShortcut::activated, this,
            &ConditionRowsView::pasteRows);
    connect(new QShortcut(QKeySequence(Qt::Key_Delete), this, nullptr, nullptr, Qt::WidgetShortcut), &QShortcut::activated, this,
            &ConditionRowsView::removeSelectedRows);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        QMenu menu(this);
        const auto actions = RowActionMenu::populate(menu, selectionModel()->selectedRows().size(), m_clipboard.size());
        connect(actions.copy, &QAction::triggered, this, &ConditionRowsView::copyRows);
        connect(actions.paste, &QAction::triggered, this, &ConditionRowsView::pasteRows);
        connect(actions.remove, &QAction::triggered, this, &ConditionRowsView::removeSelectedRows);
        menu.exec(viewport()->mapToGlobal(position));
    });
}
void ConditionRowsView::copyRows()
{
    m_clipboard.clear();
    auto rows = selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.row() < b.row(); });
    for (const auto &row : rows)
        m_clipboard << m_rows->editableRow(row.row());
}
void ConditionRowsView::pasteRows()
{
    int row = -1;
    for (const auto &index : selectionModel()->selectedRows())
        row = qMax(row, index.row());
    m_rows->insertRecords(row + 1, m_clipboard);
}
void ConditionRowsView::removeSelectedRows()
{
    QList<int> rows;
    for (const auto &index : selectionModel()->selectedRows())
        rows << index.row();
    if (rows.isEmpty() && m_rows->rowCount())
        rows << m_rows->rowCount() - 1;
    if (!rows.isEmpty())
        m_rows->removeRecords(rows);
}
