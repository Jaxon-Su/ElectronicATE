#pragma once

#include <QString>

struct XmlOperationResult {
    enum class Error { None, Open, Write, Parse, InvalidRoot };
    Error error = Error::None;
    QString detail;
    qint64 line = 0;

    bool succeeded() const { return error == Error::None; }
};
