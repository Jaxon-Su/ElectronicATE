#include "commandconsoleviewmodel.h"
#include "communicationfactory.h"
#include "icommunication.h"

CommandConsoleViewModel::CommandConsoleViewModel(CommandConsoleModel* model, QObject* parent)
    : CommandConsoleViewModel(
          model,
          [](const QString& address) {
              return std::unique_ptr<ICommunication>(CommunicationFactory::create(address));
          },
          parent)
{
}
