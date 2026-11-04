#pragma once
#include <QString>

// The family selects a command set; the selected model number stays in XML and in the driver.
enum class ACSourceFamily { Unknown, DeltaA3000, Chroma61505, Chroma61507_61508_61509, Chroma6512_6520_6530,
                            Chroma6415_6420_6430, Chroma6560_6590 };

inline ACSourceFamily acSourceFamily(const QString &model)
{
    if (model == "DE-A3000AB") return ACSourceFamily::DeltaA3000;
    if (model == "61505") return ACSourceFamily::Chroma61505;
    if (model == "61507" || model == "61508" || model == "61509") return ACSourceFamily::Chroma61507_61508_61509;
    if (model == "6512" || model == "6520" || model == "6530") return ACSourceFamily::Chroma6512_6520_6530;
    if (model == "6415" || model == "6420" || model == "6430") return ACSourceFamily::Chroma6415_6420_6430;
    if (model == "6560" || model == "6590") return ACSourceFamily::Chroma6560_6590;
    return ACSourceFamily::Unknown;
}
