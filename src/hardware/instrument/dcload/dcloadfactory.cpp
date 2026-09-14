#include "dcloadfactory.h"
#include "chroma6310.h"
#include "chroma6310a.h"
#include "chroma63600.h"
#include "chroma63200a.h"
#include "chroma63804.h"
#include "loadcapabilitycatalog.h"

DCLoadFactory::DCLoadFactory() {}

DCLoad* DCLoadFactory::createDCLoad(const QString& modelName, ICommunication* comm)
{
    if (!comm) return nullptr;

    if (LoadCapabilityCatalog::Chroma6310Models.contains(modelName))
        return new Chroma6310(modelName, comm);

    if (LoadCapabilityCatalog::Chroma6310AModels.contains(modelName))
        return new Chroma6310A(modelName, comm);

    if (LoadCapabilityCatalog::Chroma63600Models.contains(modelName))
        return new Chroma63600(modelName, comm);

    if (LoadCapabilityCatalog::Chroma63200AModels.contains(modelName))
        return new Chroma63200A(modelName, comm);

    if (LoadCapabilityCatalog::Chroma63800Models.contains(modelName))
        return new Chroma63804(comm);

    return nullptr;
}

QStringList DCLoadFactory::supportedManualModes(const QString& modelName, const QString& baseMode)
{
    return LoadCapabilityCatalog::supportedManualModes(modelName, baseMode);
}

QStringList DCLoadFactory::supportedDynamicManualModes(const QString& modelName)
{
    return LoadCapabilityCatalog::supportedDynamicManualModes(modelName);
}