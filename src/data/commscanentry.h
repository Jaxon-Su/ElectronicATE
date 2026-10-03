#pragma once
#include <QJsonObject>
#include <QString>

struct CommScanEntry {
    QString transport;
    QString address;
    QString description;
    QString source;
    QString visaAddress;

    QJsonObject toJson() const
    {
        return {{"transport", transport}, {"address", address},
                {"description", description}, {"source", source}, {"visaAddress", visaAddress}};
    }
    static CommScanEntry fromJson(const QJsonObject &object)
    {
        return {object.value("transport").toString(), object.value("address").toString(),
                object.value("description").toString(), object.value("source").toString(),
                object.value("visaAddress").toString()};
    }
    QString displayAddress() const
    {
        return visaAddress.isEmpty() ? address : QString("%1 (%2)").arg(address, visaAddress);
    }
    QString key() const
    {
        QString key = address.trimmed().toUpper();
        key.replace("TCPIP::", "TCPIP0::");
        key.replace("::INST0::INSTR", "::INSTR");
        return key;
    }
};
