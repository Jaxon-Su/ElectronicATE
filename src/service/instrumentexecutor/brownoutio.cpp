#include "brownoutio.h"
#include "acinputconfiguration.h"
#include "acsourcefactory.h"
#include "dcsourcefactory.h"
#include "dcloadfactory.h"
#include "dcsourcespec/chroma62000spec.h"
#include "loadcapabilitycatalog.h"
#include "conditionvalues.h"
#include <array>
#include <stdexcept>

namespace {
void require(bool ok, const QString &error)
{ if (!ok) throw std::invalid_argument(error.toStdString()); }
struct Plan {
    Page5ExecutionContext context;
    BrownOut::Settings settings;
    InstrumentConfig ac, monitor;
    int channel = 0;
    std::array<InstrumentConfig, 3> dc;
    std::array<bool, 3> useDc{};
    std::array<double, 3> volts{}, amps{}, resolution{};
    ParameterParser::InputParameters input;
    bool rampAc = false;
    bool useAc = false;
    double initial = 0;
};

class BrownOutIo final : public IBrownOutIo
{
  public:
    BrownOutIo(Plan plan, InstrumentOperations operations,
               std::function<ICommunication *(const QString &)> create)
        : m_plan(std::move(plan)), m_operations(std::move(operations)), m_create(std::move(create)) {}
    void start(const std::function<bool()> &cancelled) override
    {
        const auto check = [&] { require(!cancelled(), "Stopped during Brown Out startup"); };
        check();
        if (m_plan.useAc) {
            m_acComm = connectTransport(m_plan.ac);
            m_ac.reset(ACSourceFactory::createACSource(m_plan.ac.modelName, m_acComm.get()));
            connectDriver(m_ac.get());
            m_ac->setPowerOff();
            check();
            require(configureACInput(m_ac.get(), m_plan.input), "Unsupported AC phase mode");
            verifyVoltage(m_ac.get(), m_plan.input.isThreePhase() ? m_plan.input.voltage / std::sqrt(3.0) : m_plan.input.voltage, 0.1);
        }
        for (int i = 0; i < 3; ++i) {
            if (!m_plan.useDc[i]) continue;
            check();
            m_dcComm[i] = connectTransport(m_plan.dc[i]);
            m_dc[i].reset(DCSourceFactory::createDCSource(m_plan.dc[i].modelName, m_dcComm[i].get()));
            connectDriver(m_dc[i].get());
            m_dc[i]->setPowerOff();
            check();
            m_dc[i]->setCurrent(m_plan.amps[i]);
            m_dc[i]->setVoltage(m_plan.volts[i]);
            verifyVoltage(m_dc[i].get(), m_plan.volts[i], m_plan.resolution[i]);
        }
        check();
        const auto &ctx = m_plan.context;
        auto loaded = m_operations.load(ctx.page1, ctx.loads, m_plan.settings.load, ctx.loadMeta,
                                        LoadAction::LoadOn, ctx.loadSync.enabled);
        require(loaded.success, loaded.errorMessage);
        check();
        m_monitorComm = connectTransport(m_plan.monitor);
        m_monitor.reset(DCLoadFactory::createDCLoad(m_plan.monitor.modelName, m_monitorComm.get()));
        connectDriver(m_monitor.get());
        m_monitor->setChannel(m_plan.channel);
        // Check the independent monitor before enabling the DUT input.
        (void)outputVoltage();
        check();
        if (m_ac) {
            m_ac->setPowerOn();
        }
        for (int i = 0; i < 3; ++i) {
            if (!m_dc[i]) continue;
            check();
            m_dc[i]->setPowerOn();
        }
    }
    double initialVoltage() const override { return m_plan.initial; }
    void setVoltage(double voltage) override
    {
        require(std::isfinite(voltage) && voltage >= m_plan.settings.minimumVin && voltage <= m_plan.initial,
                "Brown Out voltage is outside the planned descent");
        if (m_plan.rampAc) {
            const double phaseVoltage = m_plan.input.isThreePhase() ? voltage / std::sqrt(3.0) : voltage;
            m_ac->setVoltageWhileOn(phaseVoltage);
            verifyVoltage(m_ac.get(), phaseVoltage, 0.1);
        } else {
            auto *source = m_dc[m_plan.settings.dcSource - 1].get();
            source->setVoltage(voltage);
            verifyVoltage(source, voltage, m_plan.resolution[m_plan.settings.dcSource - 1]);
        }
    }
    double outputVoltage() override
    {
        double voltage = 0;
        QString error;
        require(m_monitor && m_monitor->readOutputVoltage(voltage, error), error);
        return voltage;
    }
    bool shutdown(QString &error) override
    {
        QStringList failures;
        auto attempt = [&](auto action) {
            try { action(); }
            catch (const std::exception &e) { failures.append(QString::fromUtf8(e.what())); }
            catch (...) { failures.append("Unexpected Brown Out output cleanup error"); }
        };
        if (m_plan.useAc) attempt([&] {
            if (!m_ac || !m_ac->isConnected()) {
                m_ac.reset(); m_acComm.reset();
                m_acComm = connectTransport(m_plan.ac);
                m_ac.reset(ACSourceFactory::createACSource(m_plan.ac.modelName, m_acComm.get()));
                connectDriver(m_ac.get());
            }
            m_ac->setPowerOff();
        });
        for (int i = 0; i < 3; ++i)
            if (m_plan.useDc[i]) attempt([&] {
                if (!m_dc[i] || !m_dc[i]->isConnected()) {
                    m_dc[i].reset(); m_dcComm[i].reset();
                    m_dcComm[i] = connectTransport(m_plan.dc[i]);
                    m_dc[i].reset(DCSourceFactory::createDCSource(m_plan.dc[i].modelName, m_dcComm[i].get()));
                    connectDriver(m_dc[i].get());
                }
                m_dc[i]->setPowerOff();
            });
        // Release the monitor connection before the existing Load OFF operation opens its session.
        m_monitor.reset(); m_monitorComm.reset();
        // Earlier steady tasks may have left outputs ON, even if start() failed
        // before reaching this source or the load in the current task.
        attempt([&] {
            const auto &ctx = m_plan.context;
            const auto off = m_operations.load(ctx.page1, ctx.loads, m_plan.settings.load, ctx.loadMeta,
                                               LoadAction::LoadOff, ctx.loadSync.enabled);
            require(off.success, off.errorMessage);
        });
        error = failures.join('\n'); return failures.isEmpty();
    }
  private:
    template<class Source> static void verifyVoltage(Source *source, double expected, double resolution)
    {
        double actual = 0;
        QString error;
        require(source->readVoltageSetting(actual, error), error);
        // Digital setting resolution, not analogue output accuracy; allow one rounding half-step.
        require(std::abs(actual - expected) <= resolution * 0.51 + 1e-6,
                QString("Source did not accept voltage setting: requested %1 V, read back %2 V")
                    .arg(expected, 0, 'g', 10).arg(actual, 0, 'g', 10));
    }
    std::unique_ptr<ICommunication> connectTransport(const InstrumentConfig &config)
    {
        std::unique_ptr<ICommunication> comm(m_create(config.getResourceString()));
        require(bool(comm), "Unsupported Brown Out communication address");
        comm->setTimeoutMs(1000);
        return comm;
    }
    static void connectDriver(InstrumentWithCommBase *driver)
    {
        require(driver != nullptr, "Brown Out driver creation failed");
        driver->connect();
        require(driver->isConnected(), driver->lastError());
    }
    Plan m_plan;
    InstrumentOperations m_operations;
    std::function<ICommunication *(const QString &)> m_create;
    std::unique_ptr<ICommunication> m_acComm, m_monitorComm;
    std::array<std::unique_ptr<ICommunication>, 3> m_dcComm;
    std::unique_ptr<ACSource> m_ac;
    std::unique_ptr<DCLoad> m_monitor;
    std::array<std::unique_ptr<DCSource>, 3> m_dc;
};
}

BrownOutIoFactory prepareBrownOutIo(const Page5ExecutionContext &context, const BrownOut::Settings &cfg,
                                   const InstrumentOperations &operations,
                                   std::function<ICommunication *(const QString &)> create)
{
    require(bool(operations.load) && bool(create), "Brown Out hardware adapter is unavailable");
    require(cfg.inputs.valid(context.inputs.size(), context.dcInputs.size()),
            "Select a Brown Out input condition");
    require(cfg.load >= 0 && cfg.load < context.loads.size(), "Select a Brown Out Load condition");
    Plan plan{context, cfg};
    plan.context.scope = nullptr;
    plan.useAc = cfg.inputs.ac >= 0;
    plan.rampAc = cfg.rampSource == BrownOut::RampSource::AC;
    require((plan.rampAc ? cfg.inputs.ac : cfg.inputs.dc) >= 0, "Select the Brown Out ramp source input condition");
    auto instrument = [&](const QString &name, const QString &type) {
        InstrumentConfig found; int count = 0;
        for (const auto &item : context.page1.instruments)
            if (item.enabled && item.type == type && (name.isEmpty() || item.name == name)) {
                found = item; ++count;
            }
        require(count == 1 && !found.getResourceString().isEmpty(), "Configure one enabled " + type + " " + name);
        return found;
    };
    QStringList resources;
    if (plan.useAc) {
        plan.ac = instrument({}, "InputSource");
        require(QStringList{"61505", "61509", "6530"}.contains(plan.ac.modelName),
                "Brown Out requires Chroma 61505 / 61509 / 6530 programmed-voltage readback");
        plan.input = ParameterParser::parseInputRow(context.inputs[cfg.inputs.ac]);
        require(plan.input.valid && std::isfinite(plan.input.voltage) && std::isfinite(plan.input.frequency) &&
                    std::isfinite(plan.input.phase) && plan.input.frequency > 0 &&
                    plan.input.phase >= 0 && plan.input.phase <= 360, "Invalid Brown Out AC condition");
        require(!plan.input.isThreePhase() || plan.ac.modelName == "61509", "3phase requires Chroma 61509");
        const double phaseVoltage = plan.input.isThreePhase() ? plan.input.voltage / std::sqrt(3.0) : plan.input.voltage;
        require(phaseVoltage <= (plan.ac.modelName == "61509" ? 350 : 300) &&
                    plan.input.frequency >= 15 && plan.input.frequency <= (plan.ac.modelName == "61505" ? 1000 : 2000),
                "Brown Out AC voltage/frequency exceeds the supported model range");
        if (plan.rampAc) {
            plan.initial = plan.input.voltage;
            const double phaseStep = plan.input.isThreePhase() ? cfg.voltageStep / std::sqrt(3.0) : cfg.voltageStep;
            require(phaseStep + 1e-9 >= 0.1, "Brown Out AC step must be at least 0.1 V per phase");
        }
        resources << plan.ac.getResourceString();
    }
    if (cfg.inputs.dc >= 0) {
        const auto &group = context.dcInputs[cfg.inputs.dc];
        bool anyDc = false;
        for (int i = 0; i < qBound(1, context.page1.dcInputs, 3); ++i) {
            const auto name = QString("DC Source%1").arg(i + 1);
            bool enabled = false;
            for (const auto &item : context.page1.instruments)
                enabled |= item.enabled && item.type == "InputDCSource" && item.name == name;
            if (!enabled) continue;
            plan.dc[i] = instrument(name, "InputDCSource"); plan.useDc[i] = true; anyDc = true;
            const auto spec = Chroma62000Spec::forModel(plan.dc[i].modelName);
            require(spec.has_value(), "Unsupported DC model for Brown Out");
            // 62000H digital-interface voltage resolution: 0.002% of full-scale voltage.
            plan.resolution[i] = spec->maxVoltage * 0.00002;
            require(ConditionValues::finiteNumber(group[i].vin, plan.volts[i]) && plan.volts[i] >= 0 && plan.volts[i] <= spec->maxVoltage &&
                    ConditionValues::finiteNumber(group[i].currentLimit, plan.amps[i]) && plan.amps[i] >= 0 && plan.amps[i] <= spec->maxCurrent,
                    "Invalid Brown Out DC condition or model rating");
            const auto resource = plan.dc[i].getResourceString();
            require(!resources.contains(resource, Qt::CaseInsensitive), "AC/DC sources must have distinct addresses");
            resources << resource;
        }
        require(anyDc, "Enable at least one DC source for the selected DC input condition");
        if (!plan.rampAc) {
            require(plan.useDc[cfg.dcSource - 1], "Enable the selected DC source to ramp");
            plan.initial = plan.volts[cfg.dcSource - 1];
            require(cfg.voltageStep + 1e-9 >= plan.resolution[cfg.dcSource - 1],
                    "Brown Out step is below the selected DC source voltage resolution");
        }
    }
    require(std::isfinite(plan.initial) && plan.initial > cfg.minimumVin,
            "Initial Vin must exceed minimum Vin");
    int matches = 0;
    for (const auto &load : context.page1.instruments) {
        if (!load.enabled || load.type != "Load") continue;
        for (int slot = 0; slot < load.channels.size(); ++slot) {
            if (load.channels[slot].index != cfg.monitorOutput) continue;
            ++matches; plan.monitor = load; plan.monitor.modelName = load.channels[slot].subModel;
            plan.channel = load.channelNumbers.value(slot, -1);
        }
    }
    require(matches == 1 && plan.channel > 0 && !plan.monitor.getResourceString().isEmpty(),
            "Map the monitor Load output Index to exactly one enabled channel in Page1");
    require(!resources.contains(plan.monitor.getResourceString(), Qt::CaseInsensitive), "Input and monitor must use distinct addresses");
    const bool is6310 = LoadCapabilityCatalog::Chroma6310Models.contains(plan.monitor.modelName) ||
                        LoadCapabilityCatalog::Chroma6310AModels.contains(plan.monitor.modelName);
    require(is6310 || LoadCapabilityCatalog::Chroma63600Models.contains(plan.monitor.modelName) ||
            LoadCapabilityCatalog::Chroma63200AModels.contains(plan.monitor.modelName),
            "Brown Out live Vout monitoring supports Chroma 6310 / 6310A / 63600 / 63200A");
    require(!is6310 || plan.channel <= 8, "Brown Out 6310 / 6310A monitor channel must be 1-8");
    require(!context.loads[cfg.load].values.value(cfg.monitorOutput - 1).trimmed().isEmpty(),
            "The selected Load condition is missing the monitor output value");
    return [plan, operations, create = std::move(create)] { return std::make_unique<BrownOutIo>(plan, operations, create); };
}
