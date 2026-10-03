#include "dcsourcefactory.h"
#include "chroma62000.h"

DCSource* DCSourceFactory::createDCSource(const QString& modelName, ICommunication* comm)
{
    if (!comm || !Chroma62000Spec::forModel(modelName))
        return nullptr;
    return new Chroma62000(modelName, comm);
}
