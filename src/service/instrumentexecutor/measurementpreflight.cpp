#include "measurementpreflight.h"
#include "acinputvalidation.h"
#include "dcinputvalidation.h"
#include "conditionvalues.h"
#include "loadcapabilitycatalog.h"
#include "chromaload6310aspec.h"
#include "chromaload63600spec.h"
#include "chromaload63200aspec.h"
#include <limits>
#include <stdexcept>

// 6310 and 6310A catalogs currently declare the same type names; isolate the
// older catalog in its own translation unit rather than aliasing their ratings.
void validate6310Rating(const QString &model, double current, double voltage, const QString &label);

namespace {
void require(bool ok, const QString &error)
{
    if (!ok) throw std::invalid_argument(error.toStdString());
}
void checkError(const QString &error) { require(error.isEmpty(), error); }

template<class Range>
void checkRating(const std::optional<Range> &range, double current, double voltage, const QString &label)
{
    require(range.has_value(), label + ": unknown load rating");
    require(current <= range->currentSpec.maxCurrent && voltage <= range->voltageSpec.maxVoltage &&
                current * voltage <= range->power, label + ": load current/voltage/power exceeds the model rating");
}
} // namespace

void validateMeasurementConditions(const Page5ExecutionContext &context, InputSelection inputs,
                                   int loadIndex, bool dynamic)
{
    require(inputs.valid(context.inputs.size(), context.dcInputs.size()), "Select valid AC/DC conditions");
    QStringList inputResources;
    const InstrumentConfig *ac = nullptr;
    for (const auto &instrument : context.page1.instruments) {
        if (!instrument.enabled || instrument.type != "InputSource" || instrument.name != "ACSource") continue;
        require(!ac, "Duplicate ACSource configuration");
        ac = &instrument;
    }
    if (ac) { // Even None requires a usable OFF path for an enabled source.
        require(!ac->getResourceString().trimmed().isEmpty() &&
                    QStringList{"61505", "61509", "6530", "DE-A3000AB"}.contains(ac->modelName),
                "Configure the enabled ACSource model and address");
        inputResources << ac->getResourceString().trimmed();
    }
    if (inputs.ac >= 0) {
        require(ac, "Enable ACSource for the selected AC condition");
        checkError(validateAcParameters(ac->modelName, ParameterParser::parseInputRow(context.inputs[inputs.ac])));
    }
    DcInputPlan dc;
    if (inputs.dc >= 0) {
        checkError(prepareDcInputs(context.page1, context.dcInputs[inputs.dc], dc));
    } else {
        // OFF does not depend on a DC condition, but all configured sources must be reachable by a driver.
        for (int source = 0; source < qBound(1, context.page1.dcInputs, 3); ++source)
            for (const auto &instrument : context.page1.instruments) {
                if (!instrument.enabled || instrument.type != "InputDCSource" ||
                    instrument.name != QString("DC Source%1").arg(source + 1)) continue;
                require(!dc.sources[source], "Duplicate DC Source configuration");
                dc.sources[source] = &instrument;
                require(Chroma62000Spec::forModel(instrument.modelName).has_value() &&
                            !instrument.getResourceString().trimmed().isEmpty(), "Configure enabled DC Source model/address");
                dc.resources << instrument.getResourceString().trimmed();
            }
    }
    for (const auto &resource : dc.resources) {
        require(!inputResources.contains(resource, Qt::CaseInsensitive), "AC/DC sources must use distinct addresses");
        inputResources << resource;
    }
    require(loadIndex >= 0 && loadIndex < (dynamic ? context.dynamics.size() : context.loads.size()),
            "Select a valid load condition");
    const auto &values = dynamic ? context.dynamics[loadIndex].values : context.loads[loadIndex].values;
    const auto &vo = dynamic ? context.dynamicMeta.vo : context.loadMeta.vo;
    const auto &von = dynamic ? context.dynamicMeta.von : context.loadMeta.von;
    const auto &ranges = dynamic ? context.dynamicMeta.ranges : context.loadMeta.ranges;
    if (dynamic) {
        const auto times = context.dynamicMeta.t1t2.value(loadIndex).split('~');
        require(times.size() <= 2, "Dynamic timing requires one or two durations");
        for (const auto &time : times) {
            double value = 0;
            require(ConditionValues::finiteNumber(time, value) && value > 0 && value / 1000.0 > 0,
                    "Dynamic durations must be finite and positive (ms)");
        }
    }
    int mapped = 0;
    QSet<QString> physicalChannels;
    for (const auto &instrument : context.page1.instruments) {
        if (!instrument.enabled || instrument.type != "Load") continue;
        const auto resource = instrument.getResourceString().trimmed();
        require(!resource.isEmpty() && !instrument.modelName.isEmpty(), instrument.name + ": configure Load model/address");
        require(!inputResources.contains(resource, Qt::CaseInsensitive), "Input and load must use distinct addresses");
        for (int slot = 0; slot < instrument.channels.size(); ++slot) {
            const auto &channel = instrument.channels[slot];
            if (channel.index == 0) continue;
            const QString label = instrument.name + QString(" output %1").arg(channel.index);
            const int index = channel.index - 1;
            require(index >= 0 && index < values.size(), label + ": missing load condition value");
            const auto model = channel.subModel;
            const bool c6310 = LoadCapabilityCatalog::Chroma6310Models.contains(model);
            const bool c6310a = LoadCapabilityCatalog::Chroma6310AModels.contains(model);
            const bool c63600 = LoadCapabilityCatalog::Chroma63600Models.contains(model);
            const bool c63200 = LoadCapabilityCatalog::Chroma63200AModels.contains(model);
            const bool c63800 = LoadCapabilityCatalog::Chroma63800Models.contains(model);
            require(c6310 || c6310a || c63600 || c63200 || c63800, label + ": unsupported load model");
            const int hw = instrument.channelNumbers.value(slot, -1);
            require(hw > 0 && (!(c6310 || c6310a) || hw <= 8), label + ": invalid hardware channel");
            const auto key = resource.toUpper() + '/' + QString::number(hw);
            require(!physicalChannels.contains(key), label + ": duplicate hardware channel");
            physicalChannels.insert(key);
            ++mapped;
            const auto mode = dynamic ? QString("CC") : context.loadMeta.modes.value(index, "CC").trimmed().toUpper();
            require(mode == "CC" || mode == "CV", label + ": unsupported load mode");
            require(mode != "CV" || c63200, label + ": CV programming is not implemented for this model");
            const auto range = ranges.value(index).trimmed().toUpper();
            const auto supported = dynamic ? LoadCapabilityCatalog::supportedDynamicManualModes(model)
                                           : LoadCapabilityCatalog::supportedManualModes(model, mode);
            require(range.isEmpty() || range == "AUTO RANGE" || range == "NO SETTING" || supported.contains(range),
                    label + ": unsupported load range");
            double voltage = 0, current = 0, threshold = 0;
            if (!vo.value(index).trimmed().isEmpty())
                require(ConditionValues::finiteNumber(vo[index], voltage) && voltage >= 0, label + ": invalid Vo");
            if (!von.value(index).trimmed().isEmpty())
                require(ConditionValues::finiteNumber(von[index], threshold) && threshold >= 0, label + ": invalid Von");
            if (dynamic) {
                const auto levels = values[index].split('~');
                require(levels.size() <= 2, label + ": unsupported dynamic segment count");
                for (const auto &level : levels) {
                    double value = 0;
                    require(ConditionValues::finiteNumber(level, value) && value >= 0, label + ": invalid dynamic current");
                    current = qMax(current, value);
                }
            } else if (mode == "CV") {
                require(ConditionValues::cvPair(values[index], voltage, current), label + ": invalid CV voltage/current");
            } else {
                require(ConditionValues::nonNegativeWithUnit(values[index], 'A', current), label + ": invalid CC current");
            }
            // Existing catalog data, full model limits (range selection remains the driver's responsibility).
            const auto max = std::numeric_limits<double>::max();
            if (c6310) validate6310Rating(model, current, voltage, label);
            if (c6310a) checkRating(findPowerRange6310A(model, max), current, voltage, label);
            if (c63600) checkRating(findPowerRange63600(model, max), current, voltage, label);
            if (c63200) checkRating(findPowerRange63200A(model, max), current, voltage, label);
        }
    }
    require(mapped > 0, "Enable and map at least one load channel");
}
