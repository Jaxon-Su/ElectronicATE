#include "commscanner.h"
#include "../../data/commscanentry.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSerialPortInfo>
#include <QSet>
#include <cstdio>
#include <visa.h>

namespace {
void send(const QJsonObject &object)
{
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout);
    std::fflush(stdout);
}

void discover()
{
    QSet<QString> comPorts;
    for (const auto &port : QSerialPortInfo::availablePorts()) {
        comPorts.insert(port.portName().toUpper());
        QStringList description{port.description(), port.manufacturer()};
        description.removeAll(QString());
        const CommScanEntry entry{"Serial", port.portName(), description.join(" / "), "System"};
        send({{"entry", entry.toJson()}});
    }

    ViSession manager = VI_NULL;
    ViStatus status = viOpenDefaultRM(&manager);
    if (status < VI_SUCCESS) {
        send({{"warning", QString("VISA resource manager unavailable (%1).").arg(status)}});
        return;
    }
    const auto closeManager = qScopeGuard([&] { viClose(manager); });
    ViFindList list = VI_NULL;
    ViUInt32 count = 0;
    ViChar resource[VI_FIND_BUFLEN] = {};
    status = viFindRsrc(manager, ViString("?*"), &list, &count, resource);
    const auto closeList = qScopeGuard([&] { if (list) viClose(list); });
    if (status == VI_ERROR_RSRC_NFOUND)
        return;
    if (status < VI_SUCCESS) {
        send({{"warning", QString("VISA discovery failed (%1).").arg(status)}});
        return;
    }
    for (ViUInt32 i = 0; i < count && i < 512; ++i) {
        if (i && (status = viFindNext(list, resource)) < VI_SUCCESS) {
            send({{"warning", QString("VISA discovery stopped (%1).").arg(status)}});
            break;
        }
        const QString address = QString::fromLatin1(resource);
        const auto transport = address.section("::", 0, 0).remove(QRegularExpression("[0-9]+$"));
        CommScanEntry entry{transport, address, {}, "VISA"};
        if (transport.compare("ASRL", Qt::CaseInsensitive) == 0) {
            ViUInt16 interfaceType = 0, interfaceNumber = 0;
            ViChar resourceClass[VI_FIND_BUFLEN] = {}, expanded[VI_FIND_BUFLEN] = {}, alias[VI_FIND_BUFLEN] = {};
            const auto parsed = viParseRsrcEx(manager, resource, &interfaceType, &interfaceNumber,
                                            resourceClass, expanded, alias);
            const QString port = QString::fromLatin1(alias).trimmed().toUpper();
            if (parsed == VI_SUCCESS && interfaceType == VI_INTF_ASRL && comPorts.contains(port)) {
                entry.transport = "Serial";
                entry.address = port;
                entry.visaAddress = address;
            }
        }
        send({{"entry", entry.toJson()}});
    }
    if (count > 512)
        send({{"warning", "Only the first 512 VISA resources were returned."}});
}
} // namespace

int runCommScanner()
{
    discover();
    send({{"done", true}});
    return 0;
}
