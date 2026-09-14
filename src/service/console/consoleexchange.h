#pragma once
#include <QString>
#include <functional>

class ICommunication;

struct ConsoleExchangeResult {
    enum class Error { None, Disconnected, Write, Read, Timeout, Interrupted, NoResponse, Exception };
    Error error = Error::None;
    QString response;
    QString detail;
};

// continueWaiting may service owner-thread events. It must return false if the
// borrowed connection was released/replaced; no transport access follows false.
ConsoleExchangeResult exchangeConsoleCommand(ICommunication& connection,
    const QString& command, bool expectResponse, int timeoutMs,
    const std::function<bool()>& continueWaiting = {});
