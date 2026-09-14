#pragma once
#include <QSet>
#include <QStringList>
namespace LoadCapabilityCatalog {
inline const QSet<QString> Chroma6310Models = {
    "63101", "63102", "63103", "63105", "63106", "63108", "63112"
};

inline const QSet<QString> Chroma6310AModels = {
    "63101A", "63102A", "63103A", "63105A", "63106A", "63107A",
    "63108A", "63110A", "63112A", "63113A", "63115A", "63123A"
};

inline const QSet<QString> Chroma63600Models = {
    "63610-80-20", "63630-80-60", "63640-80-80", "63640-150-60", "63630-600-15"
};

inline const QSet<QString> Chroma63200AModels = {
    // 150V 系列
    "63202A-150-200", "63203A-150-300", "63204A-150-400",
    "63205A-150-500", "63206A-150-600", "63208A-150-800",
    // 600V 系列
    "63202A-600-140", "63205A-600-350", "63206A-600-420",
    "63224A-600-1680",
    // 1200V 系列
    "63202A-1200-80", "63204A-1200-160", "63205A-1200-200",
    "63206A-1200-240", "63208A-1200-320",
};

inline const QSet<QString> Chroma63800Models = {
    "63802", "63803", "63804"
};

inline QStringList supportedManualModes(const QString& modelName, const QString& baseMode)
{
    const QString mode = baseMode.trimmed().toUpper();

    if (Chroma6310Models.contains(modelName)) {
        if (mode == "CC") return {"CCL", "CCH"};
        if (mode == "CV") return {"CV"};
        return {};
    }

    if (Chroma6310AModels.contains(modelName)) {
        if (mode == "CC") return {"CCL", "CCH"};
        if (mode == "CV") return {"CV"};
        return {};
    }

    if (Chroma63600Models.contains(modelName)) {
        if (mode == "CC") return {"CCL", "CCM", "CCH"};
        if (mode == "CV") return {"CVL", "CVM", "CVH"};
        return {};
    }

    if (Chroma63200AModels.contains(modelName)) {
        if (mode == "CC") return {"CCL", "CCM", "CCH"};
        if (mode == "CV") return {"CVL", "CVM", "CVH"};
        return {};
    }

    if (Chroma63800Models.contains(modelName)) {
        if (mode == "CC") return {"CURR"};
        if (mode == "CV") return {"VOLT"};
        return {};
    }

    return {};
}

inline QStringList supportedDynamicManualModes(const QString& modelName)
{
    if (Chroma6310Models.contains(modelName) || Chroma6310AModels.contains(modelName))
        return {"CCDL", "CCDH"};

    if (Chroma63600Models.contains(modelName) || Chroma63200AModels.contains(modelName))
        return {"CCDL", "CCDM", "CCDH"};

    if (Chroma63800Models.contains(modelName))
        return {"CURR"};

    return {};
}

}
