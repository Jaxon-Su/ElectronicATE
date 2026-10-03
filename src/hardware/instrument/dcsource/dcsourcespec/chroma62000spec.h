#pragma once
#include <QString>
#include <QStringList>
#include <optional>

// Single-unit ratings. Voltage/current settings both have a minimum of zero.
struct DCSourceSpec {
    QString model;
    double maxVoltage; // V
    double maxCurrent; // A
    double maxPower;   // W
};
namespace Chroma62000Spec {
std::optional<DCSourceSpec> forModel(const QString& model);
QStringList supportedModels();
}
