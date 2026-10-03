#pragma once
#include <QString>
#include "acsource.h"
#include "icommunication.h"

class ACSourceFactory {
  public:
    static ACSource* createACSource(const QString& modelName, ICommunication* comm);
};
