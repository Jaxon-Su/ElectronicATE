#pragma once

#include <QList>
#include <QString>
#include "ixmlserializable.h"
#include "xmloperationresult.h"

// Stateless XML persistence. Page composition and error presentation belong to
// the caller; this service can run without widgets or a global QObject instance.
class XmlConfigStore final {
public:
    [[nodiscard]] static XmlOperationResult saveAllToXml(
        const QString& fileName, const QList<IXmlSerializable*>& pages);
    [[nodiscard]] static XmlOperationResult loadAllFromXml(
        const QString& fileName, const QList<IXmlSerializable*>& pages);

private:
    XmlConfigStore() = delete;
};
