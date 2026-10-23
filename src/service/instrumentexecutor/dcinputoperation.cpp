#include "dcinputoperation.h"
#include "dcinputvalidation.h"
#include "dcsourcefactory.h"
#include "dcsourcespec/chroma62000spec.h"
#include <cmath>
#include <memory>
#include <stdexcept>

InstrumentOperationResult
runDcInput(const InstrumentSetupConfig& config, int source, const DcRow& row, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication)
{
    bool outputWriteAttempted = false;
    bool observedEnabled = false;
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
            return {false, name + ": enable the instrument in InstrumentSetup first.", true};
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
                        name + ": enter valid Vin (V) and I Limit (A) within the model ratings in TestConditions.",
                        true};
        }
        // Destruction order matters: the driver borrows the transport.
        std::unique_ptr<ICommunication> comm(createCommunication(resource));
        if (!comm)
            return {false, name + ": unsupported communication resource.", true};
        comm->setTimeoutMs(selected->commConfig.timeout);
        std::unique_ptr<DCSource> driver(DCSourceFactory::createDCSource(selected->modelName, comm.get()));
        if (!driver)
            return {false, name + ": driver creation failed.", true};
        driver->setAddress(resource);
        driver->connect();
        if (!driver->isConnected())
            return {false, name + ": " + driver->lastError(), true};
        if (action == InputAction::PowerOff) {
            outputWriteAttempted = true; // An OFF readback failure also leaves the actual state unknown.
            driver->setPowerOff();
        } else {
            observedEnabled = driver->outputEnabled();
            // Limit current before applying voltage. Change never enables output.
            driver->setCurrent(current);
            driver->setVoltage(voltage);
            if (action == InputAction::PowerOn) {
                outputWriteAttempted = true;
                driver->setPowerOn();
            }
        }
        return {true, {}, false, observedEnabled ? std::optional<bool>(true) : std::nullopt};
    } catch (const std::exception& error) {
        return {false, QString("DC Source%1: %2").arg(source + 1).arg(QString::fromUtf8(error.what())),
                !outputWriteAttempted, !outputWriteAttempted && observedEnabled ? std::optional<bool>(true) : std::nullopt};
    } catch (...) {
        // Group OFF must continue to the remaining sources for any transport failure.
        return {false, QString("DC Source%1: unexpected operation failure").arg(source + 1), !outputWriteAttempted,
                !outputWriteAttempted && observedEnabled ? std::optional<bool>(true) : std::nullopt};
    }
}

InstrumentOperationResult
runDcGroup(const InstrumentSetupConfig& config, const DcGroup& rows, InputAction action,
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
            return {false, "Enable at least one DC Source in InstrumentSetup.", true};
        if (!errors.isEmpty())
            return {false, errors.join('\n')};
        return {true, {}, false, false};
    }
    DcInputPlan plan;
    const auto validationError = prepareDcInputs(config, rows, plan);
    if (!validationError.isEmpty()) return {false, validationError, true};
    const auto &selected = plan.sources;
    const auto &volts = plan.volts;
    const auto &amps = plan.amps;
    // Drivers are destroyed before their borrowed transports.
    std::array<std::unique_ptr<ICommunication>, 3> comms;
    std::array<std::unique_ptr<DCSource>, 3> drivers;
    bool enableAttempted = false;
    bool observedEnabled = false;
    try {
        for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
            if (!selected[source])
                continue;
            comms[source].reset(createCommunication(selected[source]->getResourceString()));
            if (!comms[source])
                throw std::runtime_error("DC Source transport creation failed.");
            comms[source]->setTimeoutMs(selected[source]->commConfig.timeout);
            drivers[source].reset(
                DCSourceFactory::createDCSource(selected[source]->modelName, comms[source].get()));
            if (!drivers[source])
                throw std::runtime_error("DC Source driver/transport creation failed.");
            drivers[source]->connect();
            if (!drivers[source]->isConnected())
                throw std::runtime_error(drivers[source]->lastError().toStdString());
        }
        for (const auto& driver : drivers)
            if (driver) observedEnabled |= driver->outputEnabled();
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
        return {true, {}, true, observedEnabled ? std::optional<bool>(true) : std::nullopt};
    } catch (...) {
        QString message;
        try { throw; }
        catch (const std::exception& error) { message = QString::fromUtf8(error.what()); }
        catch (...) { message = "Unexpected DC group operation failure"; }
        if (!enableAttempted)
            return {false, message, true, observedEnabled ? std::optional<bool>(true) : std::nullopt};
        bool allOff = true;
        for (auto& driver : drivers) {
            if (!driver)
                continue;
            try {
                driver->setPowerOff();
            } catch (const std::exception& offError) {
                allOff = false;
                message += "\n" + QString::fromUtf8(offError.what());
            } catch (...) {
                allOff = false;
                message += "\nUnexpected DC rollback OFF failure";
            }
        }
        return {false, message, false, allOff ? std::optional<bool>(false) : std::nullopt};
    }
}
