#include "oscilloscopefactory.h"
#include <QDebug>

Oscilloscope* OscilloscopeFactory::createOscilloscope(const QString& modelName, ICommunication* comm)
{
    if (!comm) {
        qWarning() << "[OscilloscopeFactory] Communication interface is null";
        return nullptr;
    }

    const QString model = modelName.toUpper().trimmed();

    // 使用工廠模式創建對應的示波器實例
    if (model == "DPO7000") {
        return new DPO7000(comm);
    }
    // 未來可以輕鬆添加新型號
    else if (model == "DPO5000") {
        // return new DPO5000(comm);
        qWarning() << "[OscilloscopeFactory] DPO5000 not implemented yet";
        return nullptr;
    }
    else if (model == "MSO4000") {
        // return new MSO4000(comm);
        qWarning() << "[OscilloscopeFactory] MSO4000 not implemented yet";
        return nullptr;
    }

    qWarning() << "[OscilloscopeFactory] Unsupported oscilloscope model:" << modelName;
    return nullptr;
}

QStringList OscilloscopeFactory::getSupportedModels()
{
    return {
        "DPO7000",
        "DPO5000",  // 預留
        "MSO4000"   // 預留
    };
}

bool OscilloscopeFactory::isModelSupported(const QString& modelName)
{
    return getSupportedModels().contains(modelName.toUpper().trimmed());
}

QString OscilloscopeFactory::getVendorByModel(const QString& modelName)
{
    const QString model = modelName.toUpper().trimmed();

    if (model.startsWith("DPO") || model.startsWith("MSO") || model.startsWith("TDS")) {
        return "Tektronix";
    }
    else if (model.startsWith("DSO") || model.startsWith("MSA")) {
        return "Keysight";
    }
    else if (model.startsWith("RTM") || model.startsWith("RTO")) {
        return "Rohde & Schwarz";
    }

    return "Unknown";
}
