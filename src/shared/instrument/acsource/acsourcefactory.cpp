#include "acsourcefactory.h"

ACSourceFactory::ACSourceFactory() {}

ACSource* ACSourceFactory::createACSource(const QString& modelName, ICommunication* comm)
{
    if (!comm) return nullptr;
    if (modelName == "DE-A3000AB") return new DeltaA3000(comm);
    return nullptr;

    return nullptr;
}
