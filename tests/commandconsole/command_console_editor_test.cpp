#include "../../src/views/commandconsole/commandconsolepage.h"
#include "commandconsoleviewmodel.h"
#include "commandconsolemodel.h"
#include "icommunication.h"
#include <QApplication>
#include <QPlainTextEdit>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <iostream>
#include <stdexcept>
void check(bool ok) { if (!ok) throw std::runtime_error("CommandConsole offline editor regression"); }
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        CommandConsoleModel model;
        CommandConsoleViewModel vm(&model, [](const QString&) -> std::unique_ptr<ICommunication> { return {}; });
        CommandConsolePage page(&vm); page.show(); app.processEvents();
        auto *editor = page.findChild<QPlainTextEdit*>();
        check(editor && editor->isEnabled());
        editor->setPlainText("CHAN 3");
        QMetaObject::invokeMethod(&page, "onSendClicked", Qt::DirectConnection);
        check(editor->toPlainText() == "CHAN 3");
        bool clearFound = false;
        QTimer::singleShot(0, [&] {
            auto *menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            for (auto *action : menu->actions()) if (action->text() == "Clear") {
                clearFound = action->isEnabled(); action->trigger(); break;
            }
            menu->close();
        });
        QMetaObject::invokeMethod(editor, "customContextMenuRequested", Qt::DirectConnection, Q_ARG(QPoint, QPoint(5,5)));
        check(clearFound && editor->toPlainText().isEmpty());
        editor->setPlainText("*IDN?");
        page.findChild<QPushButton*>("clearCommandButton")->click();
        check(editor->toPlainText().isEmpty());
        std::cout << "PASS: offline edit, context Clear, button Clear and unsent text retention\n";
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
