#include "../../service/connection/connectiontestservice.h"
#include "communicationfactory.h"

std::unique_ptr<ConnectionTestService> makeConnectionTestService()
{
    return std::make_unique<ConnectionTestService>(
        [](const QString &resource)
        { return std::unique_ptr<ICommunication>(CommunicationFactory::create(resource)); });
}
