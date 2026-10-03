#pragma once

#include <QString>
#include <QStringList>

// Presentation/controller family selection. This is deliberately distinct
// from hardware creation and instrument channel-count capabilities.
namespace TriggerModelCatalog {
enum class Family { Unknown, Mso456 };

inline const QStringList& mso456Models()
{
    static const QStringList models{"MSO44",  "MSO44B", "MSO46",  "MSO46B", "MSO54",       "MSO54B",
                                    "MSO56",  "MSO56B", "MSO58",  "MSO58B", "MSO58LP",     "MSO64",
                                    "MSO64B", "MSO66B", "MSO68B", "LPD64",  "MSOSeries456"};
    return models;
}

inline Family family(const QString& modelName)
{
    const QString normalized = modelName.trimmed().toUpper();
    if (mso456Models().contains(normalized, Qt::CaseInsensitive))
        return Family::Mso456;
    return Family::Unknown;
}

inline QStringList supportedModels()
{
    return mso456Models();
}

inline int channelCount(const QString& modelName)
{
    const auto model = modelName.trimmed().toUpper();
    if (QStringList{"MSO46", "MSO46B", "MSO56", "MSO56B", "MSO66B"}.contains(model))
        return 6;
    if (QStringList{"MSO58", "MSO58B", "MSO58LP", "MSO68B"}.contains(model))
        return 8;
    return 4;
}

inline bool isSupported(const QString& modelName)
{
    return family(modelName) != Family::Unknown;
}

inline QString familyName(const QString& modelName)
{
    switch (family(modelName)) {
    case Family::Mso456:
        return QStringLiteral("MSOSeries456");
    case Family::Unknown:
        return modelName.trimmed().toUpper();
    }
    return {};
}
} // namespace TriggerModelCatalog
