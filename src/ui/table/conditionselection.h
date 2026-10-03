#pragma once
#include <QApplication>
#include <QContextMenuEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <QTableView>

namespace ConditionSelection {
inline void openPersistentEditor(QTableView *view, const QModelIndex &index)
{
    const bool newlyCreated = !view->indexWidget(index);
    view->openPersistentEditor(index);
    // Qt automatically selects all text in new editors, including unfocused persistent ones.
    if (newlyCreated)
        if (auto *line = qobject_cast<QLineEdit *>(view->indexWidget(index)))
            line->deselect();
}

inline void setSelected(QWidget *editor, bool selected)
{
    if (editor->property("conditionSelected").isValid() &&
        editor->property("conditionSelected").toBool() == selected)
        return;
    editor->setProperty("conditionSelected", selected);
    editor->style()->unpolish(editor);
    editor->style()->polish(editor);
    editor->update();
}

class Controller : public QObject
{
  public:
    explicit Controller(QTableView *view) : QObject(view), m_view(view)
    {
        setObjectName("conditionSelectionController");
        m_view->setProperty("conditionRangeSelection", false);
        m_view->installEventFilter(this);
        m_view->viewport()->installEventFilter(this);
        connect(view->model(), &QAbstractItemModel::modelReset, this, [this] {
            m_pressed = false;
            showRange(false);
        });
    }

    void showRange(bool visible)
    {
        if (m_view->property("conditionRangeSelection").toBool() == visible)
            return;
        m_view->setProperty("conditionRangeSelection", visible);
        m_view->style()->unpolish(m_view);
        m_view->style()->polish(m_view);
        for (const auto &index : m_view->selectionModel()->selectedIndexes())
            if (auto *editor = m_view->indexWidget(index))
                setSelected(editor, visible);
        m_view->viewport()->update();
    }

  protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        const bool viewport = object == m_view->viewport();
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            // Focusing a persistent editor here would collapse the dragged row selection.
            if (mouse->button() == Qt::RightButton && !viewport && object != m_view &&
                m_view->property("conditionRangeSelection").toBool())
                return true;
            if (mouse->button() == Qt::LeftButton) {
                // Starting another edit/selection also removes stale highlights in sibling tables.
                for (auto *table : m_view->window()->findChildren<QTableView *>())
                    if (auto *controller = table->findChild<QObject *>(
                            "conditionSelectionController", Qt::FindDirectChildrenOnly))
                        static_cast<Controller *>(controller)->showRange(false);
                showRange(false);
                m_pressed = viewport && m_view->indexAt(mouse->position().toPoint()).isValid();
                m_pressPosition = mouse->position().toPoint();
            }
        } else if (viewport && event->type() == QEvent::MouseMove) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (m_pressed && (mouse->buttons() & Qt::LeftButton) &&
                (mouse->position().toPoint() - m_pressPosition).manhattanLength() >=
                    QApplication::startDragDistance())
                showRange(true);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressed = false;
        } else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            const bool modifier = key->key() == Qt::Key_Control || key->key() == Qt::Key_Shift ||
                                  key->key() == Qt::Key_Alt || key->key() == Qt::Key_Meta;
            const bool contextMenu = key->key() == Qt::Key_Menu ||
                                     (key->key() == Qt::Key_F10 && key->modifiers() == Qt::ShiftModifier);
            if (!modifier && !contextMenu && !key->matches(QKeySequence::Copy))
                showRange(false);
        } else if (!viewport && object != m_view && event->type() == QEvent::ContextMenu &&
                   m_view->property("conditionRangeSelection").toBool()) {
            const auto *context = static_cast<QContextMenuEvent *>(event);
            // A dragged row range uses the table Copy menu, even over a persistent editor.
            emit m_view->customContextMenuRequested(m_view->viewport()->mapFromGlobal(context->globalPos()));
            return true;
        }
        return QObject::eventFilter(object, event);
    }

  private:
    QTableView *m_view;
    QPoint m_pressPosition;
    bool m_pressed = false;
};

inline void decorate(QTableView *view, QWidget *editor, const QModelIndex &index)
{
    editor->setStyleSheet(editor->styleSheet() + QStringLiteral(
        "QLineEdit { selection-background-color: #e2e5e9; selection-color: black; }"
        "QLineEdit[conditionSelected='true'], QComboBox[conditionSelected='true']"
        " { background-color: #3399ff; color: white; }"));
    if (auto *controller = view->findChild<QObject *>(
            "conditionSelectionController", Qt::FindDirectChildrenOnly))
        editor->installEventFilter(controller);
    setSelected(editor, view->property("conditionRangeSelection").toBool() &&
                            view->selectionModel()->isSelected(index));
}
inline void install(QTableView *view)
{
    auto *controller = new Controller(view);
    view->setStyleSheet(view->styleSheet() + QStringLiteral(
        "QTableView[conditionRangeSelection='true']::item:selected"
        " { background-color: #3399ff; color: white; }"));
    QObject::connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, view,
        [view, controller](const QItemSelection &selected, const QItemSelection &deselected) {
            for (const auto &index : deselected.indexes())
                if (auto *editor = view->indexWidget(index))
                    setSelected(editor, false);
            for (const auto &index : selected.indexes())
                if (auto *editor = view->indexWidget(index))
                    setSelected(editor, view->property("conditionRangeSelection").toBool());
            if (!view->selectionModel()->hasSelection())
                controller->showRange(false);
        });
}
} // namespace ConditionSelection
