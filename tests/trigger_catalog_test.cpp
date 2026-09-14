#include "triggermodelcatalog.h"
#include <iostream>

int main()
{
    using namespace TriggerModelCatalog;
    if (family(" mso4000 ") != Family::Dpo4000
        || familyName("mso4000") != "DPO4000"
        || family("dpo7000") != Family::Dpo7000
        || family(" mSoSeRiEs456 ") != Family::Mso456
        || !isSupported("MSOSeries456")
        || isSupported("DPO5000") || isSupported("MSO9999") || isSupported("")
        || familyName(" unknown ") != "UNKNOWN") {
        std::cerr << "Trigger family/alias classification failed\n";
        return 1;
    }
    const auto supported = supportedModels();
    if (supported.size() != 20 || supported.last() != "MSOSeries456") return 1;
    for (const auto& name : supported) {
        if (!isSupported(name) || !isSupported(" " + name.toLower() + " ")) {
            std::cerr << "Listed trigger model rejected\n";
            return 1;
        }
    }
    std::cout << "PASS: trigger families, supported-list consistency and mixed-case alias\n";
    return 0;
}
