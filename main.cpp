#include "mainwindow.h"
#include <icommunication.h>
#include <testcommunicationfactory.h>
#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // app.setStyle("windows11");

    // qDebug() << "Using style:" << app.style()->objectName();
    // qDebug() << "Available styles:" << QStyleFactory::keys();

    MainWindow w;
    w.show();
    return app.exec();
}
