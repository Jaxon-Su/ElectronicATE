#pragma once
#include <QAction>
#include <QMenu>

namespace RowActionMenu {
inline QString style()
{
    return QStringLiteral(
        "QMenu { background: #ffffff; border: 1px solid #c8d0e0; border-radius: 4px;"
        " padding: 4px 0px; font-size: 12px; }"
        "QMenu::item { padding: 6px 32px 6px 16px; color: #2c3e50; }"
        "QMenu::item:selected { background: #e8f0fe; color: #1a56db; }"
        "QMenu::item:disabled { color: #bbb; }"
        "QMenu::separator { height: 1px; background: #e0e5ef; margin: 3px 8px; }");
}
struct Actions {
    QAction *copy;
    QAction *paste;
    QAction *remove;
};
inline Actions populate(QMenu &menu, int selected, int clipboard)
{
    menu.setStyleSheet(style());
    auto *copy = menu.addAction(selected
        ? QString("📋  Copy  (%1 row%2)  Ctrl+C").arg(selected).arg(selected > 1 ? "s" : "")
        : QString("📋  Copy  Ctrl+C"));
    copy->setEnabled(selected > 0);
    auto *paste = menu.addAction(clipboard
        ? QString("📌  Paste  (%1 row%2 in clipboard)  Ctrl+V").arg(clipboard).arg(clipboard > 1 ? "s" : "")
        : QString("📌  Paste  (clipboard empty)  Ctrl+V"));
    paste->setEnabled(clipboard > 0);
    menu.addSeparator();
    auto *remove = menu.addAction(selected
        ? QString("🗑  Delete  (%1 row%2)").arg(selected).arg(selected > 1 ? "s" : "")
        : QString("🗑  Delete"));
    remove->setEnabled(selected > 0);
    return {copy, paste, remove};
}
} // namespace RowActionMenu
