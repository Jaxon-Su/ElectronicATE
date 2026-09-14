#include "page4viewmodel.h"
#include "communicationfactory.h"
#include "icommunication.h"

Page4ViewModel::Page4ViewModel(Page4Model* model, QObject* parent)
    : Page4ViewModel(model, [](const QString& address) {
          return std::unique_ptr<ICommunication>(CommunicationFactory::create(address));
      }, parent)
{}
