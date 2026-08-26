#include "relayfactory.h"
#include "relay_rtu_4.h"

RelayBase* RelayFactory::createRelay(const QString& modelName, ICommunication* comm, quint8 slaveAddr) {
    if (modelName.contains("Modbus_RTU_4CH", Qt::CaseInsensitive)) {
        return new Relay_RTU_4(comm, slaveAddr);
    }

    // 未來擴充範例：
    // if (modelName.contains("TCP_8", Qt::CaseInsensitive)) {
    //     return new Relay_TCP_8(comm, 0x01);
    // }

    return nullptr;
}
