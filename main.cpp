#include "mainwindow.h"
#include <QApplication>
#include "src/hardware/communication/commscanner.h"
#include <cstring>

int main(int argc, char *argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--scan-comm-helper") == 0) {
        QCoreApplication app(argc, argv);
        return runCommScanner();
    }
    QApplication app(argc, argv);

    MainWindow *window = new MainWindow();
    window->show();

    return app.exec();
}
