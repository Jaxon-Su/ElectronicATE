#include "modbusrtuexchange.h"
#include "modbusrtucodec.h"
#include "icommunication.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QThread>
#include <stdexcept>
#include <algorithm>

ModbusExchangeResult exchangeModbusRtu(ICommunication &connection, const ModbusRequest &request,
                                      int timeoutMs, int quietMs, const std::function<bool()> &continueWaiting,
                                      const std::function<void(const QByteArray &)> &transmitting)
{
    ModbusExchangeResult result;
    try {
        const auto frame = ModbusRtuCodec::encode(request, result.error);
        if (frame.isEmpty()) return result;
        if (timeoutMs <= 0 || quietMs <= 0) throw std::runtime_error("Invalid RTU timeout/timing");
        const int originalTimeout = connection.timeoutMs();
        const auto restore = qScopeGuard([&] { connection.setTimeoutMs(originalTimeout); });
        QElapsedTimer elapsed; elapsed.start();
        auto check = [&] {
            if (continueWaiting && !continueWaiting()) throw std::runtime_error("Modbus operation cancelled");
            if (!connection.isOpen()) throw std::runtime_error("Modbus transport disconnected");
            if (elapsed.elapsed() >= timeoutMs) throw std::runtime_error("Modbus response timeout / incomplete frame");
        };
        auto read = [&](int waitMs) {
            check();
            connection.setTimeoutMs(qMax(1, std::min({waitMs, 50, timeoutMs - int(elapsed.elapsed())})));
            QByteArray chunk;
            const int count = connection.read(chunk, 256);
            if (count < 0 && !connection.readTimedOut())
                throw std::runtime_error(("Modbus read failed: " + connection.lastError()).toStdString());
            if (count > 256 || (count >= 0 && count != chunk.size()))
                throw std::runtime_error("Invalid transport byte count");
            if (count > 0) return chunk;
            // Also yield when a test/custom transport returns immediately with no bytes.
            QThread::msleep(1);
            return QByteArray{};
        };
        check();
        if (!connection.discardInput())
            throw std::runtime_error(("Cannot clear RTU input: " + connection.lastError()).toStdString());
        // Require a silent interval before transmitting; drain late bytes from an older transaction.
        QElapsedTimer quiet; quiet.start();
        while (quiet.elapsed() < quietMs) {
            if (!read(quietMs - int(quiet.elapsed())).isEmpty()) quiet.restart();
            check();
        }
        check();
        connection.setTimeoutMs(qMax(1, timeoutMs - int(elapsed.elapsed())));
        result.tx = frame;
        if (transmitting) transmitting(frame);
        const int written = connection.write(frame); // binary bytes, with CRC and no text terminator
        if (written != frame.size())
            throw std::runtime_error(QString("Modbus write failed (%1/%2 bytes): %3")
                                         .arg(written).arg(frame.size()).arg(connection.lastError()).toStdString());
        check();
        bool complete = false;
        for (;;) {
            const auto chunk = read(complete ? quietMs - int(quiet.elapsed()) : 50);
            result.rx += chunk;
            check();
            const int expected = ModbusRtuCodec::responseSize(request, result.rx);
            if (result.rx.size() > expected) throw std::runtime_error("Extra bytes after Modbus response");
            // Serial/USB delivery gaps do not measure wire-level inter-character timing.
            // Collect the expected frame within this transaction's deadline, then check for trailing bytes.
            if (!complete && result.rx.size() == expected) { complete = true; quiet.restart(); }
            if (complete && quiet.elapsed() >= quietMs) break;
        }
        ModbusRtuCodec::decode(request, result.rx, result.summary, result.error);
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    } catch (...) {
        result.error = "Unknown Modbus communication failure";
    }
    return result;
}
