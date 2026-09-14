#include "consoleexchange.h"
#include "icommunication.h"
#include <QElapsedTimer>
#include <QThread>
#include <exception>

ConsoleExchangeResult exchangeConsoleCommand(ICommunication& connection,
    const QString& command, bool expectResponse, int timeoutMs,
    const std::function<bool()>& continueWaiting)
{
    using Error = ConsoleExchangeResult::Error;
    try {
        if (!connection.isOpen()) return {Error::Disconnected, {}, {}};
        QByteArray encoded = command.toUtf8();
        if (!encoded.endsWith('\n')) encoded += '\n';
        if (connection.write(encoded) != encoded.size())
            return {Error::Write, {}, connection.lastError()};
        if (!expectResponse) return {};

        QByteArray response;
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < timeoutMs) {
            QByteArray chunk;
            const int count = connection.read(chunk, 4096);
            if (count < 0) return {Error::Read, {}, connection.lastError()};
            if (count > 0) {
                response.append(chunk);
                if (response.contains('\n')) break;
            }
            QThread::msleep(10);
            if (continueWaiting && !continueWaiting()) return {Error::Interrupted, {}, {}};
        }
        if (response.isEmpty()) return {Error::Timeout, {}, {}};
        const QString text = QString::fromUtf8(response).trimmed();
        return {text.isEmpty() ? Error::NoResponse : Error::None, text, {}};
    } catch (const std::exception& error) {
        return {Error::Exception, {}, QString::fromUtf8(error.what())};
    } catch (...) {
        return {Error::Exception, {}, QStringLiteral("Unknown communication error")};
    }
}
