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
            !query("MEASUrement:LIST?", list, error))
            return false;
        state = state.toUpper();
        m_stopAfter = m_stopAfter.toUpper();
        if (!QStringList{"0", "1", "STOP", "RUN", "OFF", "ON"}.contains(state) ||
            !QStringList{"RUNSTOP", "SEQUENCE"}.contains(m_stopAfter))
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
        auto restoreCommand = [&](const QString &command)
        {
            QString failure;
            bool restored = false;
            try
            {
                restored = m_scope.writeConfigurationCommand(command, failure);
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
        restoreCommand("ACQuire:STATE STOP");
        if (m_created)
            restoreCommand("MEASUrement:DELete \"" + m_id + "\"");
        restoreCommand("ACQuire:STOPAfter " + m_stopAfter);
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
    QString m_source, m_stopAfter, m_id;
    double m_divisions = 0;
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
