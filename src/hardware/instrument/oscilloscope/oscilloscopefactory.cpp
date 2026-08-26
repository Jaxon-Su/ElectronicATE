#include "oscilloscopefactory.h"
#include <QDebug>

Oscilloscope* OscilloscopeFactory::createOscilloscope(const QString& modelName, ICommunication* comm)
{
    if (!comm) {
        qWarning() << "[OscilloscopeFactory] Communication interface is null";
        return nullptr;
    }

    const QString model = modelName.toUpper().trimmed();

    if (model == "DPO7000") {
        return new DPO7000(comm);
    }
    else if (model == "DPO4000") {
        return new DPO4000(comm);
    }
    else if (model == "MSO44"  || model == "MSO44B"  ||
             model == "MSO46"  || model == "MSO46B"  ||
             model == "MSO54"  || model == "MSO54B"  ||
             model == "MSO56"  || model == "MSO56B"  ||
             model == "MSO58"  || model == "MSO58B"  ||
             model == "MSO58LP"||
             model == "MSO64"  || model == "MSO64B"  ||
             model == "MSO66B" || model == "MSO68B"  ||
             model == "LPD64") {
        auto* osc = new MSOSeries456(comm);

        // 依機型自動設定通道數
        if (model == "MSO46"  || model == "MSO46B"  ||
            model == "MSO56"  || model == "MSO56B"  ||
            model == "MSO66B" || model == "MSO64"   || model == "MSO64B") {
            osc->setTotalChannel(6);
        } else if (model == "MSO58"  || model == "MSO58B"  ||
                   model == "MSO58LP"|| model == "MSO68B") {
            osc->setTotalChannel(8);
        } else {
            // MSO44(B) / MSO54(B) / LPD64 → 4 通道
            osc->setTotalChannel(4);
        }
        return osc;
    }
    else if (model == "DPO5000") {
        // return new DPO5000(comm);
        qWarning() << "[OscilloscopeFactory] DPO5000 not implemented yet";
        return nullptr;
    }

    qWarning() << "[OscilloscopeFactory] Unsupported oscilloscope model:" << modelName;
    return nullptr;
}

QStringList OscilloscopeFactory::getSupportedModels()
{
    return {
        "DPO7000",
        "DPO4000",
        "MSO4000",
        // MSO 4/5/6 Series
        "MSO44",  "MSO44B",
        "MSO46",  "MSO46B",
        "MSO54",  "MSO54B",
        "MSO56",  "MSO56B",
        "MSO58",  "MSO58B",  "MSO58LP",
        "MSO64",  "MSO64B",
        "MSO66B", "MSO68B",
        "LPD64",
        "DPO5000"
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
