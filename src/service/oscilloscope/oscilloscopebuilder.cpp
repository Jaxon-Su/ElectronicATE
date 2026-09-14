#include "oscilloscopebuilder.h"
#include "oscilloscope.h"
#include <QDebug>
#include <exception>

OscilloscopeManager::OscMap buildConnectedOscilloscopes(
    const Page1Config& config, const OscilloscopeCreator& create)
{
    OscilloscopeManager::OscMap result;
    if (!create) return result;
    for (const auto& instrument : config.instruments) {
        if (instrument.type != "Oscilloscope" || !instrument.enabled
                || instrument.modelName.isEmpty()) continue;
        const QString resource = instrument.getResourceString();
        if (resource.isEmpty()) continue;
        try {
            auto scope = create(instrument.modelName, resource);
            if (!scope) continue;
            scope->connect();
            if (!scope->isConnected()) continue;
            result[instrument.modelName] = std::move(scope);
        } catch (const std::exception& error) {
            qWarning() << "[OscilloscopeBuilder]" << instrument.modelName << error.what();
        } catch (...) {
            qWarning() << "[OscilloscopeBuilder]" << instrument.modelName << "unknown connection error";
        }
    }
    return result;
}
