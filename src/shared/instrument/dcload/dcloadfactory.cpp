#include "dcloadfactory.h"
#include "chroma6310.h"
#include <QSet>

const QSet<QString> Chroma6310Models = {
    "63101", "63102", "63103", "63105", "63106", "63108", "63112"
};

DCLoadFactory::DCLoadFactory() {}

DCLoad* DCLoadFactory::createDCLoad(const QString& modelName, ICommunication* comm)
{
    if (!comm) return nullptr;

    if (Chroma6310Models.contains(modelName))
        return new Chroma6310(modelName, comm);
    return nullptr;

}

