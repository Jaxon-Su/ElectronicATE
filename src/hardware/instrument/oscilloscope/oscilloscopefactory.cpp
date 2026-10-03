#include "oscilloscopefactory.h"
#include "triggermodelcatalog.h"
#include <QDebug>

Oscilloscope* OscilloscopeFactory::createOscilloscope(const QString& modelName, ICommunication* comm)
{
    if (!comm) {
        qWarning() << "[OscilloscopeFactory] Communication interface is null";
        return nullptr;
    }

    const QString model = modelName.toUpper().trimmed();

    if (model == "MSO44" || model == "MSO44B" || model == "MSO46" || model == "MSO46B" ||
               model == "MSO54" || model == "MSO54B" || model == "MSO56" || model == "MSO56B" ||
               model == "MSO58" || model == "MSO58B" || model == "MSO58LP" || model == "MSO64" ||
               model == "MSO64B" || model == "MSO66B" || model == "MSO68B" || model == "LPD64") {
        auto* osc = new MSOSeries456(comm);

        osc->setTotalChannel(TriggerModelCatalog::channelCount(model));
        return osc;
    }

    qWarning() << "[OscilloscopeFactory] Unsupported oscilloscope model:" << modelName;
    return nullptr;
}

QStringList OscilloscopeFactory::getSupportedModels()
{
    return {
        // MSO 4/5/6 Series
        "MSO44",  "MSO44B",  "MSO46", "MSO46B", "MSO54",  "MSO54B", "MSO56", "MSO56B", "MSO58",
        "MSO58B", "MSO58LP", "MSO64", "MSO64B", "MSO66B", "MSO68B", "LPD64"
    };
}

bool OscilloscopeFactory::isModelSupported(const QString& modelName)
{
    return getSupportedModels().contains(modelName.toUpper().trimmed());
}

QString OscilloscopeFactory::getVendorByModel(const QString& modelName)
{
    const QString model = modelName.toUpper().trimmed();

    if (model.startsWith("DPO") || model.startsWith("MSO") || model.startsWith("TDS"))
        return "Tektronix";
    else if (model.startsWith("DSO") || model.startsWith("MSA"))
        return "Keysight";
    else if (model.startsWith("RTM") || model.startsWith("RTO"))
        return "Rohde & Schwarz";

    return "Unknown";
}
