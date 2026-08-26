#pragma once

#include <QString>
#include "page2config.h"

class ParameterParser {
public:

    struct InputParameters {
        QString phaseMode = "1phase";
        double voltage = 0.0;
        double frequency = 0.0;
        double phase = 0.0;
        bool valid = false;

        bool isThreePhase() const { return phaseMode.compare("3phase", Qt::CaseInsensitive) == 0; }
    };

    static InputParameters parseInput(const QString& text);
    static InputParameters parseInputRow(const InputRow& row);

private:

    ParameterParser() = delete;
    ~ParameterParser() = delete;
    ParameterParser(const ParameterParser&) = delete;
    ParameterParser& operator=(const ParameterParser&) = delete;
};
