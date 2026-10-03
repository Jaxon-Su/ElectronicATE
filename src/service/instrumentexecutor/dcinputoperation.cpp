#include "dcinputoperation.h"
#include "dcsourcefactory.h"
#include "dcsourcespec/chroma62000spec.h"
#include <cmath>
#include <memory>
#include <stdexcept>

InstrumentOperationResult
runDcInput(const Page1Config& config, int source, const DcRow& row, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication)
{
    bool enableAttempted = false;
    try {
        if (source < 0 || source >= config.dcInputs || source >= 3)
            return {false, "Invalid DC Source index.", true};
        const QString name = QString("DC Source%1").arg(source + 1);
        const InstrumentConfig* selected = nullptr;
        for (const auto& instrument : config.instruments) {
            if (instrument.type == "InputDCSource" && instrument.name == name) {
                if (selected)
                    return {false, name + ": duplicate instrument configuration.", true};
                selected = &instrument;
            }
        }
        if (!selected || !selected->enabled)
            return {false, name + ": enable the instrument in Page1 first.", true};
        const auto spec = Chroma62000Spec::forModel(selected->modelName);
        if (!spec)
            return {false, name + ": unsupported DC Source model.", true};
        const auto resource = selected->getResourceString();
        if (resource.trimmed().isEmpty())
            return {false, name + ": address is missing.", true};
        double voltage = 0, current = 0;
        if (action != InputAction::PowerOff) {
            bool voltageOk = false, currentOk = false;
            voltage = row.vin.trimmed().toDouble(&voltageOk);
            current = row.currentLimit.trimmed().toDouble(&currentOk);
            if (!voltageOk || !currentOk || !std::isfinite(voltage) || !std::isfinite(current) ||
                voltage < 0 || voltage > spec->maxVoltage || current < 0 || current > spec->maxCurrent)
                return {false,
                        name + ": enter valid Vin (V) and I Limit (A) within the model ratings in Page2.",
                        true};
        }
        // Destruction order matters: the driver borrows the transport.
        std::unique_ptr<ICommunication> comm(createCommunication(resource));
        if (!comm)
            return {false, name + ": unsupported communication resource.", true};
        std::unique_ptr<DCSource> driver(DCSourceFactory::createDCSource(selected->modelName, comm.get()));
        if (!driver)
            return {false, name + ": driver creation failed.", true};
        driver->setAddress(resource);
        driver->connect();
        if (!driver->isConnected())
            return {false, name + ": " + driver->lastError(), true};
        if (action == InputAction::PowerOff) {
            driver->setPowerOff();
        } else {
            // Limit current before applying voltage. Change never enables output.
            driver->setCurrent(current);
            driver->setVoltage(voltage);
            if (action == InputAction::PowerOn) {
                enableAttempted = true;
                driver->setPowerOn();
            }
        }
        return {};
    } catch (const std::exception& error) {
        return {false, QString("DC Source%1: %2").arg(source + 1).arg(QString::fromUtf8(error.what())),
                !enableAttempted};
    }
}

InstrumentOperationResult
runDcGroup(const Page1Config& config, const DcGroup& rows, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication)
{
    if (action == InputAction::PowerOff) {
        QStringList errors;
        int attempted = 0;
        for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
            bool enabled = false;
            for (const auto& instrument : config.instruments)
                if (instrument.type == "InputDCSource" &&
                    instrument.name == QString("DC Source%1").arg(source + 1))
                    enabled |= instrument.enabled;
            if (!enabled)
                continue;
            ++attempted;
            const auto result = runDcInput(config, source, {}, action, createCommunication);
            if (!result.success)
                errors.append(result.errorMessage);
        }
        if (!attempted)
            return {false, "Enable at least one DC Source in Page1.", true};
        if (!errors.isEmpty())
            return {false, errors.join('\n')};
        return {true, {}, false, false};
    }
    std::array<const InstrumentConfig*, 3> selected{};
    std::array<double, 3> volts{}, amps{};
    QStringList resources;
    for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
        const QString name = QString("DC Source%1").arg(source + 1);
        for (const auto& instrument : config.instruments) {
            if (instrument.type != "InputDCSource" || instrument.name != name)
                continue;
            if (selected[source])
                return {false, name + ": duplicate configuration.", true};
            selected[source] = &instrument;
        }
        const auto* instrument = selected[source];
        if (!instrument || !instrument->enabled) {
            selected[source] = nullptr;
            continue;
        }
        const auto spec = Chroma62000Spec::forModel(instrument->modelName);
        const auto resource = instrument->getResourceString().trimmed();
        if (!spec || resource.isEmpty())
            return {false, name + ": configure model and address in Page1.", true};
        if (resources.contains(resource, Qt::CaseInsensitive))
            return {false, name + ": DC Sources must use distinct addresses.", true};
        resources.append(resource);
        if (action != InputAction::PowerOff) {
            bool vOk = false, iOk = false;
            volts[source] = rows[source].vin.toDouble(&vOk);
            amps[source] = rows[source].currentLimit.toDouble(&iOk);
            if (!vOk || !iOk || !std::isfinite(volts[source]) || !std::isfinite(amps[source]) ||
                volts[source] < 0 || volts[source] > spec->maxVoltage || amps[source] < 0 ||
                amps[source] > spec->maxCurrent)
                return {false, name + ": invalid Vin or I Limit in the selected Page2 Index.", true};
        }
    }
    if (resources.isEmpty())
        return {false, "Enable at least one DC Source in Page1.", true};
    // Drivers are destroyed before their borrowed transports.
    std::array<std::unique_ptr<ICommunication>, 3> comms;
    std::array<std::unique_ptr<DCSource>, 3> drivers;
    bool enableAttempted = false;
    try {
        for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
            if (!selected[source])
                continue;
            comms[source].reset(createCommunication(selected[source]->getResourceString()));
            drivers[source].reset(
                DCSourceFactory::createDCSource(selected[source]->modelName, comms[source].get()));
            if (!drivers[source])
                throw std::runtime_error("DC Source driver/transport creation failed.");
            drivers[source]->connect();
            if (!drivers[source]->isConnected())
                throw std::runtime_error(drivers[source]->lastError().toStdString());
        }
        for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
            if (!drivers[source])
                continue;
            drivers[source]->setCurrent(amps[source]);
            drivers[source]->setVoltage(volts[source]);
        }
        if (action == InputAction::PowerOn) {
            for (auto& driver : drivers) {
                if (!driver)
                    continue;
                enableAttempted = true;
                driver->setPowerOn();
            }
            return {true, {}, false, true};
        }
        return {true, {}, true};
    } catch (const std::exception& error) {
        QString message = QString::fromUtf8(error.what());
        if (!enableAttempted)
            return {false, message, true};
        bool allOff = true;
        for (auto& driver : drivers) {
            if (!driver)
                continue;
            try {
                driver->setPowerOff();
            } catch (const std::exception& offError) {
                allOff = false;
                message += "\n" + QString::fromUtf8(offError.what());
            }
        }
        return {false, message, false, allOff ? std::optional<bool>(false) : std::nullopt};
    }
}
