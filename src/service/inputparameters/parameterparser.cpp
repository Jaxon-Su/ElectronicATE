#include "parameterparser.h"
#include <QStringList>
#include <QDebug>

namespace {

QString normalizePhaseMode(QString text, bool& ok)
{
    text = text.trimmed().toLower();

    if (text == "1phase") {
        ok = true;
        return "1phase";
    }

    if (text == "3phase") {
        ok = true;
        return "3phase";
    }

    ok = false;
    return {};
}

} // namespace

ParameterParser::InputParameters
ParameterParser::parseInputRow(const InputRow& row)
{
    InputParameters result;
    result.valid = false;

    bool modeOk = false;
    result.phaseMode = normalizePhaseMode(row.phaseMode, modeOk);
    if (!modeOk) {
        qWarning() << "[ParameterParser] Failed to parse phase mode:" << row.phaseMode
                   << "Expected: 1phase or 3phase";
        return result;
    }

    QString voltageStr = row.vin.trimmed();
    if (voltageStr.endsWith("V", Qt::CaseInsensitive))
        voltageStr.chop(1);
    bool voltageOk = false;
    result.voltage = voltageStr.toDouble(&voltageOk);
    if (!voltageOk)
        qWarning() << "[ParameterParser] Failed to parse voltage:" << row.vin;

    QString freqStr = row.frequency.trimmed();
    if (freqStr.endsWith("Hz", Qt::CaseInsensitive))
        freqStr.chop(2);
    bool freqOk = false;
    result.frequency = freqStr.toDouble(&freqOk);
    if (!freqOk)
        qWarning() << "[ParameterParser] Failed to parse frequency:" << row.frequency;

    QString phaseStr = row.phase.trimmed();
    if (phaseStr.endsWith("°")) {
        phaseStr.chop(1);
    } else if (phaseStr.endsWith("deg", Qt::CaseInsensitive)) {
        phaseStr.chop(3);
    }
    bool phaseOk = false;
    result.phase = phaseStr.toDouble(&phaseOk);
    if (!phaseOk)
        qWarning() << "[ParameterParser] Failed to parse phase:" << row.phase;

    result.valid = voltageOk && freqOk && phaseOk;
    if (result.valid) {
        qDebug() << "[ParameterParser] Successfully parsed:"
                 << "Mode=" << result.phaseMode << ","
                 << "Voltage=" << result.voltage << "V,"
                 << "Frequency=" << result.frequency << "Hz,"
                 << "Phase=" << result.phase << "°";
    } else {
        qWarning() << "[ParameterParser] Parse failed for input row:"
                   << row.phaseMode << row.vin << row.frequency << row.phase;
    }

    return result;
}

ParameterParser::InputParameters
ParameterParser::parseInput(const QString& text)
{
    InputParameters result;
    result.valid = false;

    // 檢查輸入是否為空
    if (text.trimmed().isEmpty()) {
        qWarning() << "[ParameterParser] Empty input text";
        return result;
    }

    // 分割字符串: "1phase/110V/60Hz/0°" 或 "3phase/528V/60Hz/0°"
    QStringList parts = text.split('/');

    if (parts.size() != 4) {
        qWarning() << "[ParameterParser] Invalid format, expected 4 parts:"
                   << text << "Got:" << parts.size();
        return result;
    }

    InputRow row;
    row.phaseMode = parts[0].trimmed();
    row.vin = parts[1].trimmed();
    row.frequency = parts[2].trimmed();
    row.phase = parts[3].trimmed();
    return parseInputRow(row);
}
