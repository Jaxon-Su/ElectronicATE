#include "oscilloscopemanager.h"
#include "oscilloscopebuilder.h"
#include "oscilloscope.h"
#include "icommunication.h"
#include "communicationfactory.h"
#include "oscilloscopefactory.h"

OscilloscopeManager::OscMap OscilloscopeManager::buildFromConfig(const InstrumentSetupConfig& config)
{
    return buildConnectedOscilloscopes(
        config, [](const QString& model, const CommunicationConfig& settings) -> std::shared_ptr<Oscilloscope> {
            auto communication = std::shared_ptr<ICommunication>(CommunicationFactory::create(settings.toResourceString()));
            if (!communication)
                return {};
            communication->setTimeoutMs(settings.timeout);
            auto* scope = OscilloscopeFactory::createOscilloscope(model, communication.get());
            if (!scope)
                return {};
            // The raw communication pointer stays valid through the scope destructor.
            return std::shared_ptr<Oscilloscope>(scope, [communication](Oscilloscope* owned) mutable {
                delete owned;
                communication.reset();
            });
        });
}
