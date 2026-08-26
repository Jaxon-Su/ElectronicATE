#include "acsourcefactory.h"
#include "deltaa3000.h"
#include "chroma61505.h"
#include "chroma61509.h"
#include "chroma6530.h"

ACSource* ACSourceFactory::createACSource(const QString& modelName, ICommunication* comm)
{
    if (!comm) return nullptr;
    if (modelName == "DE-A3000AB") return new DeltaA3000(comm);
    if (modelName == "61505")      return new Chroma61505(comm);
    if (modelName == "61509")      return new Chroma61509(comm);
    if (modelName == "6530")      return new Chroma6530(comm);
    return nullptr;
}
