#include "msoperiodsession.h"
#include "oscilloscope.h"
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace
{
QString valueToken(QString response)
{
    response = response.simplified();
    if (response.startsWith(':') || response.startsWith("MEAS", Qt::CaseInsensitive) ||
        response.startsWith("HOR", Qt::CaseInsensitive) || response.startsWith("ACQ", Qt::CaseInsensitive) ||
        response.startsWith("TRIG", Qt::CaseInsensitive) || response.startsWith("DISP", Qt::CaseInsensitive))
    {
        const int space = response.indexOf(' ');
        if (space >= 0)
            response = response.mid(space + 1);
    }
    return response.trimmed().remove('"');
}

class MsoPeriodSession final : public IScopePeriodSession
{
  public:
    MsoPeriodSession(Oscilloscope &scope, Continue ready) : m_scope(scope), m_ready(std::move(ready)) {}
    ~MsoPeriodSession() override
    {
        QString ignored;
        restore(false, ignored);
    }
    bool initialize(QString &error)
    {
        QString enabled, state, list;
        if (!query("TRIGger:A:EDGE:SOUrce?", m_source, error))
            return false;
        if (!QRegularExpression("^CH[1-9][0-9]*$").match(m_source.toUpper()).hasMatch() ||
            m_source.mid(2).toInt() > m_scope.getTotalChannel())
        {
            error = "Auto Period requires an analog CH trigger source";
            return false;
        }
        m_source = m_source.toUpper();
        QString type;
        if (!query("TRIGger:A:TYPe?", type, error) ||
            !query("TRIGger:A:EDGE:SLOpe?", m_slope, error) ||
            !numeric("TRIGger:A:LEVel:" + m_source + "?", m_originalLevel, error))
            return false;
        m_slope = m_slope.toUpper();
        if (type.toUpper() != "EDGE" || !QStringList{"RISE","FALL","EITHER"}.contains(m_slope) ||
            !std::isfinite(m_originalLevel) || std::abs(m_originalLevel) >= 1e20) {
            error = "Auto Period: invalid Edge trigger settings";
            return false;
        }
        if (!query("DISplay:GLObal:" + m_source + ":STATE?", enabled, error))
            return false;
        if (enabled != "1" && enabled.compare("ON", Qt::CaseInsensitive) != 0)
        {
            error = "Auto Period trigger channel is disabled";
            return false;
        }
        if (!numeric("HORizontal:DIVisions?", m_divisions, error))
            return false;
        if (!std::isfinite(m_divisions) || m_divisions <= 0 || m_divisions >= 1e30)
        {
            error = "Auto Period: invalid horizontal divisions";
            return false;
        }
        if (!query("ACQuire:STATE?", state, error) || !query("ACQuire:STOPAfter?", m_stopAfter, error) ||
            !query("TRIGger:A:MODe?", m_triggerMode, error) || !query("MEASUrement:LIST?", list, error))
            return false;
        state = state.toUpper();
        m_stopAfter = m_stopAfter.toUpper();
        if (!QStringList{"0", "1", "STOP", "RUN", "OFF", "ON"}.contains(state) ||
            !QStringList{"RUNSTOP", "SEQUENCE"}.contains(m_stopAfter) ||
            !QStringList{"AUTO", "NORMAL"}.contains(m_triggerMode.toUpper()))
        {
            error = "Auto Period: invalid acquisition settings";
            return false;
        }
        m_wasRunning = QStringList{"1", "RUN", "ON"}.contains(state);
        QSet<QString> used;
        if (list.toUpper() != "NONE")
        {
            for (const auto &item : list.toUpper().split(','))
            {
                const auto id = item.trimmed();
                if (!QRegularExpression("^MEAS[1-9][0-9]*$").match(id).hasMatch())
                {
                    error = "Auto Period: invalid measurement list";
                    return false;
                }
                used.insert(id);
            }
        }
        int index = 1;
        while (used.contains(QString("MEAS%1").arg(index)))
            ++index;
        m_id = QString("MEAS%1").arg(index);
        m_restoreNeeded = true;
        return true;
    }
    double horizontalDivisions() const override { return m_divisions; }
    int triggerChannel() const override { return m_source.mid(2).toInt(); }
    ScopeTriggerSlope periodSlope() const override {
        return m_slope == "FALL" ? ScopeTriggerSlope::Fall : ScopeTriggerSlope::Rise;
    }
    bool applyTriggerLevel(double requested, double &actual, QString &error) override
    {
        if (m_slope == "EITHER" && !m_slopeTouched) {
            m_slopeTouched = true;
            if (!write("TRIGger:A:EDGE:SLOpe RISE", error)) return false;
            QString slope;
            if (!query("TRIGger:A:EDGE:SLOpe?", slope, error)) return false;
            if (slope.toUpper() != "RISE") {
                error = "Auto Period: rising slope readback mismatch";
                return false;
            }
        }
        m_levelTouched = true; // A rejected/timed-out write can have reached the scope.
        const QString command = "TRIGger:A:LEVel:" + m_source;
        return write(command + " " + QString::number(requested, 'g', 16), error) &&
               numeric(command + "?", actual, error);
    }
    bool readHorizontalContext(HorizontalContext &context, QString &error) override
    {
        HorizontalContext current;
        if (!query("HORizontal:MODe?", current.mode, error)) return false;
        current.mode = current.mode.toUpper();
        if (current.mode != "AUTO" && current.mode != "MANUAL") {
            error = "Auto Period: invalid Horizontal Mode";
            return false;
        }
        if (current.mode == "MANUAL") {
            if (!query("HORizontal:MODe:MANual:CONFigure?", current.manualInputs, error)) return false;
            current.manualInputs = current.manualInputs.toUpper();
            if (current.manualInputs != "RECORDLENGTH" && current.manualInputs != "HORIZONTALSCALE") {
                error = "Auto Period: invalid Manual Inputs";
                return false;
            }
        }
        if (!numeric("HORizontal:MODe:SAMPLERate?", current.sampleRate, error) ||
            !numeric("HORizontal:MODe:RECOrdlength?", current.recordLength, error)) return false;
        if (!std::isfinite(current.sampleRate) || current.sampleRate <= 0 || current.sampleRate >= 1e30 ||
            !std::isfinite(current.recordLength) || current.recordLength < 1 || current.recordLength >= 1e30 ||
            std::floor(current.recordLength) != current.recordLength) {
            error = "Auto Period: invalid sample rate or record length";
            return false;
        }
        context = current;
        return true;
    }
    bool readTimeScale(double &value, QString &error) override
    {
        return numeric("HORizontal:SCAle?", value, error);
    }
    bool setTimeScale(double value, QString &error) override
    {
        return write("HORizontal:SCAle " + QString::number(value, 'g', 16), error);
    }
    bool prepareRecord(QString &error) override
    {
        if (!write("ACQuire:STATE STOP", error))
            return false;
        if (m_created && !write("MEASUrement:DELete \"" + m_id + "\"", error))
            return false;
        m_created = true;
        return write("MEASUrement:ADDNew \"" + m_id + "\"", error) &&
               write("MEASUrement:" + m_id + ":TYPe PERIOD", error) &&
               write("MEASUrement:" + m_id + ":SOUrce1 " + m_source, error);
    }
    bool readPeriod(double &value, QString &error) override
    {
        return numeric("MEASUrement:" + m_id + ":RESUlts:CURRentacq:MEAN?", value, error);
    }
    bool restore(bool resume, QString &error) override
    {
        if (!m_restoreNeeded)
            return true;
        m_restoreNeeded = false;
        bool ok = true;
        auto attempt = [&](auto operation)
        {
            QString failure;
            bool restored = false;
            try
            {
                restored = operation(failure);
            }
            catch (const std::exception &ex)
            {
                failure = QString::fromUtf8(ex.what());
            }
            catch (...)
            {
                failure = "Unexpected cleanup exception";
            }
            if (!restored)
            {
                if (!error.isEmpty())
                    error += "; ";
                error += "Auto Period cleanup: " + failure;
                ok = false;
            }
        };
        auto restoreCommand = [&](const QString &command) {
            attempt([&](QString &failure) { return m_scope.writeConfigurationCommand(command, failure); });
        };
        auto restoreTrigger = [&](const QString &setting, const QString &expected, bool numericLevel) {
            attempt([&](QString &failure) {
                QString response;
                if (!m_scope.writeConfigurationCommand(setting + " " + expected, failure) ||
                    !m_scope.queryConfiguration(setting + "?", response, failure)) return false;
                const QString actual = valueToken(response);
                bool valid = false;
                const double value = actual.toDouble(&valid);
                if (numericLevel ? (valid && std::isfinite(value) && qFuzzyCompare(value,m_originalLevel))
                                 : actual.compare(expected, Qt::CaseInsensitive)==0) return true;
                failure = setting + " restore mismatch: expected=" + expected + " actual=" + actual;
                return false;
            });
        };
        restoreCommand("ACQuire:STATE STOP");
        if (m_created)
            restoreCommand("MEASUrement:DELete \"" + m_id + "\"");
        if (m_levelTouched)
            restoreTrigger("TRIGger:A:LEVel:" + m_source, QString::number(m_originalLevel, 'g', 16), true);
        if (m_slopeTouched)
            restoreTrigger("TRIGger:A:EDGE:SLOpe", m_slope, false);
        restoreCommand("ACQuire:STOPAfter " + m_stopAfter);
        restoreCommand("TRIGger:A:MODe " + m_triggerMode);
        if (resume && ok && m_wasRunning && m_ready())
            restoreCommand("ACQuire:STATE RUN");
        return ok;
    }

  private:
    bool query(const QString &command, QString &response, QString &error)
    {
        if (!m_ready() || !m_scope.queryConfiguration(command, response, error))
            return false;
        response = valueToken(response);
        return m_ready();
    }
    bool numeric(const QString &command, double &value, QString &error)
    {
        QString response;
        if (!query(command, response, error))
            return false;
        bool ok = false;
        value = response.toDouble(&ok);
        if (!ok)
            error = "Auto Period: invalid response to " + command;
        return ok;
    }
    bool write(const QString &command, QString &error)
    {
        try
        {
            return m_ready() && m_scope.writeConfigurationCommand(command, error) && m_ready();
        }
        catch (const std::exception &ex)
        {
            error = "Auto Period: " + QString::fromUtf8(ex.what());
        }
        catch (...)
        {
            error = "Auto Period: configuration write failed";
        }
        return false;
    }
    Oscilloscope &m_scope;
    Continue m_ready;
    QString m_source, m_stopAfter, m_triggerMode, m_id;
    double m_divisions = 0;
    QString m_slope;
    double m_originalLevel = 0;
    bool m_levelTouched = false, m_slopeTouched = false;
    bool m_wasRunning = false, m_created = false, m_restoreNeeded = false;
};
} // namespace

std::unique_ptr<IScopePeriodSession> makeMsoPeriodSession(Oscilloscope &scope,
                                                          IScopePeriodSession::Continue ready, QString &error)
{
    auto session = std::make_unique<MsoPeriodSession>(scope, std::move(ready));
    if (!session->initialize(error))
        return {};
    return session;
}
