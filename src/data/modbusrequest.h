#pragma once
#include <QVector>
#include <QtGlobal>

// Addresses are zero-based protocol addresses, not 4xxxx register labels.
struct ModbusRequest {
    int slave = 1;
    int function = 1;
    int address = 0;
    int count = 1;
    QVector<quint16> values; // FC05/0F: 0 or 1; FC06/10: unsigned 16-bit values
};
