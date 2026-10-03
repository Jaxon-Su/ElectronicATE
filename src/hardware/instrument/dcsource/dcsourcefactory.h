#pragma once
#include <QString>
#include "dcsource.h"
#include "icommunication.h"

class DCSourceFactory {
  public:
    static DCSource* createDCSource(const QString& modelName, ICommunication* comm);
};
