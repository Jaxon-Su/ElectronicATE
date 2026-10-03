#pragma once
#include "modbusrequest.h"
#include <QByteArray>
#include <QString>

namespace ModbusRtuCodec {
bool parseValues(const QString &text, QVector<quint16> &values, QString &error);
QByteArray encode(const ModbusRequest &request, QString &error);
int responseSize(const ModbusRequest &request, const QByteArray &partial);
bool decode(const ModbusRequest &request, const QByteArray &response, QString &summary, QString &error);
}
