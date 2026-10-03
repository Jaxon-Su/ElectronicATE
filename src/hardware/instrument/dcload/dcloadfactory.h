#pragma once
#include <QString>
#include <QStringList>
#include "dcload.h"
#include "icommunication.h"

class DCLoadFactory {
  public:
    DCLoadFactory(); // 可以省略實作

    static DCLoad* createDCLoad(const QString& modelName, ICommunication* comm);
    static QStringList supportedManualModes(const QString& modelName, const QString& baseMode);
    static QStringList supportedDynamicManualModes(const QString& modelName);
};
