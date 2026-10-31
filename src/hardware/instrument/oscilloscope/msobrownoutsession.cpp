#include "msoseries456.h"
#include <QtEndian>
#include <QScopeGuard>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>

// Reads sample records, not asynchronously refreshed measurement badges. A changing
// acquisition counter invalidates the entire multi-channel read; it is never merged.
class MsoBrownOutSession final : public IScopeBrownOutSession
{
  public:
    MsoBrownOutSession(MSOSeries456 &scope, BrownOut::Settings settings, QAtomicInt &stop)
        : m_scope(scope), m_cfg(std::move(settings)), m_stop(stop) {}
    ~MsoBrownOutSession() override { QString ignored; finish(ignored); }

    bool initialize(QString &error)
    {
        try {
            if (!m_scope.m_comm || m_cfg.triggerChannel > m_scope.getTotalChannel())
                fail("Invalid Brown Out trigger channel or connection");
            for (int ch = 1; ch <= m_scope.getTotalChannel(); ++ch) {
                const QString state = read(QString("DISplay:GLObal:CH%1:STATE?").arg(ch)).toUpper();
                if (state == "1" || state == "ON") m_channels.append(ch);
                else if (state != "0" && state != "OFF") fail("Invalid channel display state");
            }
            if (!m_channels.contains(m_cfg.triggerChannel)) fail("Enable the Brown Out trigger channel first");
            const auto frames = read("HORizontal:FASTframe:STATE?").toUpper();
            if (frames != "0" && frames != "OFF") fail("Brown Out requires FastFrame disabled");
            const double length = number("HORizontal:RECOrdlength?");
            // Full records only: fail explicitly instead of silently decimating extrema.
            if (length < 2 || length > 10000000 || std::floor(length) != length)
                fail("Brown Out requires a record length from 2 to 10,000,000 samples");
            m_points = int(length);
            const double durationMs = number("HORizontal:SCAle?") * 10000;
            if (durationMs <= 0 || durationMs > std::numeric_limits<int>::max())
                fail("Invalid Brown Out acquisition duration");
            m_recordDurationMs = int(std::ceil(durationMs));
            m_step = number(QString("CH%1:SCAle?").arg(m_cfg.triggerChannel)) / 25;
            if (m_step <= 0) fail("Invalid trigger channel scale");
            for (const auto &key : QStringList{
                     "TRIGger:A:TYPe", "TRIGger:A:EDGE:SOUrce", "TRIGger:A:EDGE:SLOpe",
                     "TRIGger:A:MODe", QString("TRIGger:A:LEVel:CH%1").arg(m_cfg.triggerChannel),
                     "ACQuire:MODe",
                     "ACQuire:STOPAfter", "ACQuire:SEQuence:MODe", "ACQuire:SEQuence:NUMSEQuence",
                     "DATa:SOUrce", "DATa:ENCdg", "DATa:WIDth", "DATa:STARt", "DATa:STOP"})
                m_saved.append({key, read(key + '?')});
            m_restore = true;
            write("ACQuire:STATE STOP");
            write("TRIGger:A:TYPe EDGE");
            write(QString("TRIGger:A:EDGE:SOUrce CH%1").arg(m_cfg.triggerChannel));
            write(QString("TRIGger:A:EDGE:SLOpe %1").arg(m_cfg.rising ? "RISE" : "FALL"));
            write("ACQuire:MODe SAMPLE");
            write("ACQuire:SEQuence:MODe NUMACQS");
            write("ACQuire:SEQuence:NUMSEQuence 1");
            write("DATa:ENCdg RIBINARY");
            write("DATa:WIDth 2");
            write("DATa:STARt 1");
            write(QString("DATa:STOP %1").arg(m_points));
            // One automatic reference record after source/load settling. It is not an event result.
            QString referenceError;
            if (!m_scope.beginAutoReference(m_referenceTimeout,referenceError)) fail(referenceError);
            return true;
        } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); return false; }
    }
    bool arm(QString &error) override
    {
        return guarded(error, [&] {
            write("ACQuire:STATE STOP");
            write("TRIGger:A:MODe NORMAL");
            write((m_cfg.mode == BrownOut::Mode::FixedFall || m_cfg.captureEnabled) ? "ACQuire:STOPAfter SEQUENCE"
                                                         : "ACQuire:STOPAfter RUNSTOP");
            m_last = 0;
            m_armed = true;
            write("ACQuire:STATE RUN");
        });
    }
    bool readyToRamp(QString &error) override
    {
        error.clear();
        try {
            const auto state = read("TRIGger:STATE?").toUpper();
            if (state == "ARMED") return false;
            if (state != "READY" || counter() != 0)
                fail("Scope triggered before ramp; choose a level outside the steady waveform");
            return true;
        } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); return false; }
    }
    BrownOutRecordStatus poll(BrownOutRecord &record, QString &error) override
    {
        TransferCancellation transfer(m_stop);
        error.clear();
        record = {};
        try {
            if (!m_armed && !m_referenceFrozen) {
                const auto status = m_scope.pollAutoReference(m_referenceState,error);
                if (status == ScopeReferenceStatus::Error) fail(error);
                if (status == ScopeReferenceStatus::Pending) return BrownOutRecordStatus::Pending;
                m_referenceFrozen = true;
            }
            const bool frozen = !m_armed || m_cfg.mode == BrownOut::Mode::FixedFall || m_cfg.captureEnabled;
            if (frozen) {
                const auto state = read("ACQuire:STATE?").toUpper();
                if (state == "1" || state == "RUN" || state == "ON") return BrownOutRecordStatus::Pending;
                if (state != "0" && state != "STOP" && state != "OFF") fail("Invalid acquisition state");
            }
            const quint64 before = counter();
            if (before < m_last) fail("Acquisition counter reset during Brown Out");
            if (before == m_last) return BrownOutRecordStatus::Pending;
            BrownOutRecord candidate;
            // Preserve enough identity for a diagnostic SAVE even if CURVe?,
            // waveform metadata or clipping fails. Never expose a running record.
            if (frozen) {
                record.sequence = m_sequence + 1;
                for (int channel : m_channels) {
                    ChannelMeasurement value;
                    value.channel = channel;
                    record.channels.append(value);
                }
            }
            bool clipped = false;
            const int previousTimeout = m_scope.m_comm->timeoutMs();
            const auto restore = qScopeGuard([&] { m_scope.m_comm->setTimeoutMs(previousTimeout); });
            m_scope.m_comm->setTimeoutMs(m_cfg.dataTimeoutMs);
            for (int channel : m_channels) {
                write(QString("DATa:SOUrce CH%1").arg(channel));
                const double count = number("WFMOutpre:NR_Pt?");
                if (count != m_points) fail("Waveform length changed; partial records are not accepted");
                const double multiplier = number("WFMOutpre:YMUlt?");
                const double offset = number("WFMOutpre:YOFf?");
                const double zero = number("WFMOutpre:YZEro?");
                const QString order = read("WFMOutpre:BYT_Or?").toUpper();
                if (multiplier <= 0 || (order != "MSB" && order != "LSB")) fail("Invalid waveform encoding");
                ChannelMeasurement values;
                values.channel = channel;
                values.unit = read("WFMOutpre:YUNit?").remove('"');
                if (values.unit.isEmpty()) fail("Waveform unit is missing");
                QByteArray bytes;
                if (!m_scope.queryBinary("CURVe?", bytes) || bytes.size() != m_points * 2)
                    fail("Incomplete waveform transfer: " + m_scope.lastError());
                values.minimum = std::numeric_limits<double>::infinity();
                values.maximum = -values.minimum;
                long double sum = 0, squares = 0;
                for (int i = 0; i < m_points; ++i) {
                    const auto *p = bytes.constData() + 2 * i;
                    const qint16 raw = order == "MSB" ? qFromBigEndian<qint16>(p) : qFromLittleEndian<qint16>(p);
                    const double y = (raw - offset) * multiplier + zero;
                    if (!std::isfinite(y)) fail("Invalid waveform sample");
                    values.minimum = std::min(values.minimum, y);
                    values.maximum = std::max(values.maximum, y);
                    sum += y; squares += static_cast<long double>(y) * y;
                }
                values.mean = double(sum / m_points);
                values.rms = double(std::sqrt(squares / m_points));
                const auto clip = read(QString("CH%1:CLIPping?").arg(channel)).toUpper();
                if (clip != "0" && clip != "OFF" && clip != "1" && clip != "ON") fail("Invalid clipping state");
                clipped |= clip == "1" || clip == "ON";
                candidate.channels.append(values);
            }
            if (counter() != before) { record = {}; return BrownOutRecordStatus::Pending; }
            candidate.clipped = clipped;
            if (clipped && !m_cfg.captureEnabled) fail("Brown Out waveform is clipped; adjust vertical settings before retrying");
            m_last = before;
            candidate.sequence = ++m_sequence;
            record = std::move(candidate);
            return BrownOutRecordStatus::Ready;
        } catch (const std::exception &e) {
            error = QString::fromUtf8(e.what()); return BrownOutRecordStatus::Error;
        } catch (...) {
            error = "Unexpected Brown Out waveform read failure"; return BrownOutRecordStatus::Error;
        }
    }
    bool advanceLevel(double level, QString &error) override
    {
        return guarded(error, [&] {
            if (!std::isfinite(level)) fail("Invalid Brown Out trigger level");
            const auto command = QString("TRIGger:A:LEVel:CH%1").arg(m_cfg.triggerChannel);
            write(command + ' ' + format(level));
            const auto actual = number(command + '?');
            if (std::abs(actual - level) > std::max(m_step * 0.02, 1e-8))
                fail("Trigger level was clamped by the scope; adjust vertical settings");
            if (m_armed && m_cfg.captureEnabled && m_cfg.mode == BrownOut::Mode::Track) {
                // The previous Single is frozen until the caller finishes capture and advances.
                write("ACQuire:STATE RUN");
                m_last = 0; // RUN resets NUMACq; logical m_sequence remains monotonic.
            } else if (m_armed) m_last = counter(); // Exclude records from the previous level.
        });
    }
    double levelStep() const override { return m_step; }
    int recordDurationMs() const override { return m_recordDurationMs; }
    int referenceTimeoutMs() const override { return m_referenceTimeout; }
    QString referenceState() const override { return m_referenceState; }
    bool finish(QString &error) override
    {
        if (!m_restore) { error = m_finishError; return error.isEmpty(); }
        m_restore = false;
        m_finishing = true;
        auto attempt = [&](const QString &context, auto operation) {
            try { operation(); }
            catch (const std::exception &e) { m_finishError += context + ": " + QString::fromUtf8(e.what()) + '\n'; }
            catch (...) { m_finishError += context + ": unexpected restoration failure\n"; }
        };
        int previousTimeout = 0;
        bool haveTimeout = false;
        attempt("Read timeout", [&] { previousTimeout = m_scope.m_comm->timeoutMs(); haveTimeout = true; });
        if (haveTimeout)
            attempt("Set cleanup timeout", [&] { m_scope.m_comm->setTimeoutMs(std::min(previousTimeout, 1000)); });
        auto restore = [&](const QString &command) { attempt(command, [&] { write(command); }); };
        restore("ACQuire:STATE STOP");
        for (const auto &entry : m_saved) restore(entry.first + ' ' + entry.second);
        if (haveTimeout)
            attempt("Restore timeout", [&] { m_scope.m_comm->setTimeoutMs(previousTimeout); });
        error = m_finishError;
        return error.isEmpty();
    }
  private:
    [[noreturn]] static void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
    static QString format(double value) { return QString::number(value, 'g', 16); }
    QString read(const QString &command)
    {
        if (!m_finishing && m_stop.loadAcquire()) fail("Stopped by user");
        QString response;
        if (!m_scope.queryString(command, response)) fail(command + ": " + m_scope.lastError());
        response = response.trimmed();
        // Accept HEADER ON or OFF without changing the user's protocol preferences.
        if (response.startsWith(':') || response.startsWith(command.section(':', 0, 0).left(3), Qt::CaseInsensitive))
            response = response.section(' ', 1).trimmed();
        if (response.isEmpty() || response.contains(';') || response.contains('\n') || response.contains('\r'))
            fail("Invalid response to " + command);
        return response;
    }
    double number(const QString &command)
    {
        bool ok = false;
        const double value = read(command).toDouble(&ok);
        if (!ok || !std::isfinite(value) || std::abs(value) >= 1e30) fail("Invalid response to " + command);
        return value;
    }
    quint64 counter()
    {
        const auto value = number("ACQuire:NUMACq?");
        if (value < 0 || value > 9007199254740991.0 || std::floor(value) != value) fail("Invalid acquisition counter");
        return quint64(value);
    }
    void write(const QString &command)
    {
        if (!m_finishing && m_stop.loadAcquire()) fail("Stopped by user");
        QString error;
        if (!m_scope.writeConfigurationCommand(command, error)) fail(command + ": " + error);
    }
    template<class F> bool guarded(QString &error, F operation)
    {
        error.clear();
        try { operation(); return true; }
        catch (const std::exception &e) { error = QString::fromUtf8(e.what()); return false; }
    }
    MSOSeries456 &m_scope;
    BrownOut::Settings m_cfg;
    QAtomicInt &m_stop;
    QVector<QPair<QString, QString>> m_saved;
    QVector<int> m_channels;
    QString m_finishError;
    bool m_restore = false, m_armed = false, m_finishing = false;
    int m_points = 0, m_recordDurationMs = 0;
    int m_referenceTimeout = 0;
    QString m_referenceState;
    bool m_referenceFrozen = false;
    quint64 m_last = 0, m_sequence = 0;
    double m_step = 0;
};

std::unique_ptr<IScopeBrownOutSession> MSOSeries456::beginBrownOut(const BrownOut::Settings &cfg, QAtomicInt &stop, QString &error)
{
    auto session = std::make_unique<MsoBrownOutSession>(*this, cfg, stop);
    if (!session->initialize(error)) {
        QString cleanup;
        if (!session->finish(cleanup)) error += "\nScope restoration failed: " + cleanup;
        return {};
    }
    return session;
}
