#include "chroma62000spec.h"
#include <array>

namespace {
// 62000H Operating & Programming Manual, tables 1-1 to 1-3 (pp. 1-3 to 1-5).
const std::array<DCSourceSpec, 13> specs = {{
    {"62050H-40", 40, 125, 5000},
    {"62050H-450", 450, 11.5, 5000},
    {"62050H-600", 600, 8.5, 5000},
    {"62075H-30", 30, 250, 7500},
    {"62100H-40", 40, 250, 10000},
    {"62100H-450", 450, 23, 10000},
    {"62100H-600", 600, 17, 10000},
    {"62100H-1000", 1000, 10, 10000},
    {"62100H-30", 30, 375, 11250},
    {"62150H-40", 40, 375, 15000},
    {"62150H-450", 450, 34, 15000},
    {"62150H-600", 600, 25, 15000},
    {"62150H-1000", 1000, 15, 15000}
}};
}
std::optional<DCSourceSpec> Chroma62000Spec::forModel(const QString& model)
{
    const auto name = model.trimmed().toUpper();
    for (const auto& spec : specs)
        if (spec.model == name) return spec;
    return std::nullopt;
}
QStringList Chroma62000Spec::supportedModels()
{
    QStringList names;
    for (const auto& spec : specs) names.append(spec.model);
    return names;
}
