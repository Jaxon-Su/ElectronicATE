#pragma once
#include "instrumentsetupconfig.h"
#include "testconditionsconfig.h"
#include "dcsourcespec/chroma62000spec.h"
#include <array>
#include <cmath>

struct DcInputPlan {
    std::array<const InstrumentConfig *, 3> sources{};
    std::array<double, 3> volts{}, amps{};
    QStringList resources;
};

// Pure validation shared by batch preflight and the executor. No transports.
inline QString prepareDcInputs(const InstrumentSetupConfig &config, const DcGroup &rows, DcInputPlan &plan)
{
    for (int source = 0; source < qBound(1, config.dcInputs, 3); ++source) {
        const QString name = QString("DC Source%1").arg(source + 1);
        for (const auto &instrument : config.instruments) {
            if (instrument.type != "InputDCSource" || instrument.name != name) continue;
            if (plan.sources[source]) return name + ": duplicate configuration.";
            plan.sources[source] = &instrument;
        }
        const auto *instrument = plan.sources[source];
        if (!instrument || !instrument->enabled) { plan.sources[source] = nullptr; continue; }
        const auto spec = Chroma62000Spec::forModel(instrument->modelName);
        const auto resource = instrument->getResourceString().trimmed();
        if (!spec || resource.isEmpty()) return name + ": configure model and address in InstrumentSetup.";
        if (plan.resources.contains(resource, Qt::CaseInsensitive)) return name + ": DC Sources must use distinct addresses.";
        plan.resources << resource;
        bool vOk = false, iOk = false;
        plan.volts[source] = rows[source].vin.toDouble(&vOk);
        plan.amps[source] = rows[source].currentLimit.toDouble(&iOk);
        if (!vOk || !iOk || !std::isfinite(plan.volts[source]) || !std::isfinite(plan.amps[source]) ||
            plan.volts[source] < 0 || plan.volts[source] > spec->maxVoltage ||
            plan.amps[source] < 0 || plan.amps[source] > spec->maxCurrent)
            return name + ": invalid Vin or I Limit in the selected TestConditions Index.";
    }
    return plan.resources.isEmpty() ? "Enable at least one DC Source in InstrumentSetup." : QString{};
}
