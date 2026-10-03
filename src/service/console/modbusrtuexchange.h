#pragma once
#include "modbusrequest.h"
#include <QByteArray>
#include <QString>
#include <functional>
class ICommunication;

struct ModbusExchangeResult {
    QByteArray tx, rx;
    QString summary, error;
    bool succeeded() const { return error.isEmpty(); }
};

ModbusExchangeResult exchangeModbusRtu(ICommunication &connection, const ModbusRequest &request,
                                      int timeoutMs, int quietMs,
                                      const std::function<bool()> &continueWaiting = {},
                                      const std::function<void(const QByteArray &)> &transmitting = {});
