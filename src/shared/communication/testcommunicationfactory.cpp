#include "communicationfactory.h"
#include "icommunication.h"
#include <QDebug>

void testCommunicationFactory()
{
    // 測試 GPIB
    ICommunication* comm1 = CommunicationFactory::create("GPIB0::1::INSTR");
    if (comm1) {
        qDebug() << "GPIB instance created!";
        delete comm1;
    } else {
        qDebug() << "GPIB instance failed!";
    }

    // 測試 TCPIP
    ICommunication* comm2 = CommunicationFactory::create("TCPIP::192.168.1.100:4000");
    if (comm2) {
        qDebug() << "TCPIP instance created!";
        delete comm2;
    } else {
        qDebug() << "TCPIP instance failed!";
    }

    // 測試 Serial
    ICommunication* comm3 = CommunicationFactory::create("COM3");
    if (comm3) {
        qDebug() << "Serial instance created!";
        delete comm3;
    } else {
        qDebug() << "Serial instance failed!";
    }
}
