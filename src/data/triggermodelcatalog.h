#pragma once

#include <QString>
#include <QStringList>

// Presentation/controller family selection. This is deliberately distinct
// from hardware creation and instrument channel-count capabilities.
namespace TriggerModelCatalog {
enum class Family { Unknown, Dpo7000, Dpo4000, Mso456 };

inline const QStringList& mso456Models()
{
    static const QStringList models{
        "MSO44", "MSO44B", "MSO46", "MSO46B",
        "MSO54", "MSO54B", "MSO56", "MSO56B",
        "MSO58", "MSO58B", "MSO58LP",
        "MSO64", "MSO64B", "MSO66B", "MSO68B",
        "LPD64", "MSOSeries456"
    };
    return models;
}

inline Family family(const QString& modelName)
{
    const QString normalized = modelName.trimmed().toUpper();
    if (normalized == "DPO7000") return Family::Dpo7000;
    if (normalized == "DPO4000" || normalized == "MSO4000") return Family::Dpo4000;
    if (mso456Models().contains(normalized, Qt::CaseInsensitive)) return Family::Mso456;
    return Family::Unknown;
}

inline QStringList supportedModels()
{
    return QStringList{"DPO7000", "DPO4000", "MSO4000"} + mso456Models();
}

inline bool isSupported(const QString& modelName)
{
    return family(modelName) != Family::Unknown;
}

inline QString familyName(const QString& modelName)
{
    switch (family(modelName)) {
    case Family::Dpo7000: return QStringLiteral("DPO7000");
    case Family::Dpo4000: return QStringLiteral("DPO4000");
    case Family::Mso456: return QStringLiteral("MSOSeries456");
    case Family::Unknown: return modelName.trimmed().toUpper();
    }
    return {};
}
}
