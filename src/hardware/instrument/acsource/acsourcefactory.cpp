#include "acsourcefactory.h"
#include "acsourcefamily.h"
#include "deltaa3000.h"
#include "chroma61505.h"
#include "chroma61507_61508_61509.h"
#include "chroma6512_6520_6530.h"
#include "chroma6415_6420_6430.h"
#include "chroma6560_6590.h"

ACSource* ACSourceFactory::createACSource(const QString& modelName, ICommunication* comm)
{
    if (!comm)
        return nullptr;
    switch (acSourceFamily(modelName)) {
    case ACSourceFamily::DeltaA3000:
        return new DeltaA3000(comm);
    case ACSourceFamily::Chroma61505:
        return new Chroma61505(comm);
    case ACSourceFamily::Chroma61507_61508_61509:
        return new Chroma61507_61508_61509(modelName, comm);
    case ACSourceFamily::Chroma6512_6520_6530:
        return new Chroma6512_6520_6530(modelName, comm);
    case ACSourceFamily::Chroma6415_6420_6430:
        return new Chroma6415_6420_6430(modelName, comm);
    case ACSourceFamily::Chroma6560_6590:
        return new Chroma6560_6590(modelName, comm);
    default: return nullptr;
    }
}
