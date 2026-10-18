#include "wfmviewerviewmodel.h"
#include "../../infrastructure/waveform/tekwfmreader.h"
#include "../../infrastructure/waveform/waveformcsv.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFileInfo>
#include <QDir>
#include <exception>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
#include <locale>

WfmViewerViewModel::WfmViewerViewModel(QObject *parent) : QObject(parent)
{
    m_status = tr("Open a Tektronix analog WFM file to begin.");
    connect(&m_watcher, &QFutureWatcher<Waveform::Result>::finished, this, [this] {
        const auto result = m_watcher.result();
        m_busy = false;
        // An export that committed is complete even if Cancel was clicked after its commit point.
        const bool cancelled = result.cancelled ||
            (m_operation != Operation::Export && m_cancel->load());
        m_error.clear();
        if (cancelled) m_status = tr("Cancelled. The previous result is retained.");
        else if (!result.ok()) {
            m_error = result.error;
            m_status = tr("Operation failed. The previous result is retained.");
        } else if (m_operation == Operation::Export) {
            m_status = tr("CSV exported.");
        } else {
            m_trace = result.trace;
            initializeView();
            m_status = tr("WFM loaded.");
            emit traceChanged();
            emit viewChanged();
            emit cursorsChanged();
        }
        emit changed();
    });
}

WfmViewerViewModel::~WfmViewerViewModel()
{
    if (m_cancel) m_cancel->store(true);
    // Workers capture value snapshots only. Watcher/context destruction disconnects delivery.
}

void WfmViewerViewModel::start(Operation operation, std::function<Waveform::Result(const Cancel &)> job)
{
    if (m_busy) return;
    m_operation = operation;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_busy = true; m_error.clear();
    m_status = operation == Operation::Load ? tr("Loading WFM…") : tr("Exporting full waveform to CSV…");
    const auto cancel = m_cancel;
    m_watcher.setFuture(QtConcurrent::run([job = std::move(job), cancel] {
        try { return job(cancel); }
        catch (const std::exception &e) { return Waveform::Result{{}, QString::fromUtf8(e.what())}; }
        catch (...) { return Waveform::Result{{}, QStringLiteral("Unexpected waveform operation failure.")}; }
    }));
    emit changed();
}

void WfmViewerViewModel::load(const QString &path)
{
    if (path.isEmpty()) return;
    start(Operation::Load, [path](const Cancel &cancel) { return Waveform::readTekWfm(path, cancel); });
}

namespace {
// Calculate in long double so valid extreme trace values cannot overflow intermediate viewport math.
std::pair<double, double> paddedAxis(double first, double last, long double padding)
{
    const long double limit = std::numeric_limits<double>::max();
    return {double(std::max(-limit, static_cast<long double>(first) - padding)),
            double(std::min(limit, static_cast<long double>(last) + padding))};
}

double interpolate(double first, double last, long double ratio)
{
    return double(static_cast<long double>(first) + (static_cast<long double>(last) - first) * ratio);
}
}

void WfmViewerViewModel::initializeView()
{
    auto time = std::make_pair(m_trace.start, m_trace.end());
    if (time.first == time.second)
        time = paddedAxis(time.first, time.second,
                          std::max({static_cast<long double>(m_trace.interval), 1e-12L,
                                    std::abs(static_cast<long double>(m_trace.start)) * 1e-6L}));
    const auto &s = m_trace.statistics;
    const long double spread = static_cast<long double>(s.maximum) - s.minimum;
    const auto value = paddedAxis(s.minimum, s.maximum, spread > 0 ? spread * .08L :
                                  std::max(1e-12L, std::abs(static_cast<long double>(s.minimum)) * .1L));
    m_fullRange = {time.first, time.second, value.first, value.second};
    m_range = m_fullRange;
    m_cursor1 = interpolate(m_trace.start, m_trace.end(), .25L);
    m_cursor2 = interpolate(m_trace.start, m_trace.end(), .75L);
    m_amplitude1 = interpolate(m_fullRange.minimum, m_fullRange.maximum, .25L);
    m_amplitude2 = interpolate(m_fullRange.minimum, m_fullRange.maximum, .75L);
}

void WfmViewerViewModel::fit()
{
    if (m_busy || !hasTrace()) return;
    m_range = m_fullRange;
    emit viewChanged();
}

void WfmViewerViewModel::constrainView(long double from, long double to, long double minimum, long double maximum)
{
    if (m_busy || !hasTrace() || !std::isfinite(from) || !std::isfinite(to) ||
        !std::isfinite(minimum) || !std::isfinite(maximum) || from >= to || minimum >= maximum) return;
    const auto restrictAxis = [](long double low, long double high, double fullLow, double fullHigh, long double minSpan) {
        const long double fullSpan = static_cast<long double>(fullHigh) - fullLow;
        const long double span = std::clamp(high - low, std::min(fullSpan, minSpan), fullSpan);
        const long double center = low + (high - low) / 2;
        low = std::clamp(center - span / 2, static_cast<long double>(fullLow), static_cast<long double>(fullHigh) - span);
        return std::make_pair(double(low), double(low + span));
    };
    const auto time = restrictAxis(from, to, m_fullRange.from, m_fullRange.to,
        std::max(static_cast<long double>(m_trace.pointInterval()),
                 (static_cast<long double>(m_fullRange.to) - m_fullRange.from) * 1e-12L));
    const auto value = restrictAxis(minimum, maximum, m_fullRange.minimum, m_fullRange.maximum,
        (static_cast<long double>(m_fullRange.maximum) - m_fullRange.minimum) * 1e-12L);
    if (time.first >= time.second || value.first >= value.second) return;
    m_range = {time.first, time.second, value.first, value.second};
    emit viewChanged();
}

void WfmViewerViewModel::zoomTo(const WaveformViewRange &range)
{
    constrainView(range.from, range.to, range.minimum, range.maximum);
}

void WfmViewerViewModel::zoom(double factor, double timeAnchor, double valueAnchor)
{
    if (!std::isfinite(factor) || factor <= 0 || !std::isfinite(timeAnchor) || !std::isfinite(valueAnchor)) return;
    const long double t = std::clamp(timeAnchor, m_range.from, m_range.to);
    const long double y = std::clamp(valueAnchor, m_range.minimum, m_range.maximum);
    constrainView(t + (m_range.from - t) * factor, t + (m_range.to - t) * factor,
                  y + (m_range.minimum - y) * factor, y + (m_range.maximum - y) * factor);
}

void WfmViewerViewModel::pan(double timeDelta, double valueDelta)
{
    if (!std::isfinite(timeDelta) || !std::isfinite(valueDelta)) return;
    constrainView(static_cast<long double>(m_range.from) + timeDelta, static_cast<long double>(m_range.to) + timeDelta,
                  static_cast<long double>(m_range.minimum) + valueDelta, static_cast<long double>(m_range.maximum) + valueDelta);
}

void WfmViewerViewModel::setCursor(int cursor, double time)
{
    if (m_busy || !hasTrace() || !std::isfinite(time) || (cursor != 1 && cursor != 2)) return;
    double &current = cursor == 1 ? m_cursor1 : m_cursor2;
    current = std::clamp(time, m_trace.start, m_trace.end());
    emit cursorsChanged();
}

void WfmViewerViewModel::setAmplitudeCursor(int cursor, double value)
{
    if (m_busy || !hasTrace() || !std::isfinite(value) || (cursor != 1 && cursor != 2)) return;
    double &current = cursor == 1 ? m_amplitude1 : m_amplitude2;
    current = std::clamp(value, m_fullRange.minimum, m_fullRange.maximum);
    emit cursorsChanged();
}

long double WfmViewerViewModel::deltaAmplitude() const
{
    return std::abs(static_cast<long double>(m_amplitude2) - m_amplitude1);
}

QString WfmViewerViewModel::formatAmplitude(long double value, const QString &unit, int precision)
{
    // A finite pair of double-valued cursors can have a difference larger than DBL_MAX.
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setprecision(precision) << value;
    return QString::fromStdString(text.str()) + (unit.isEmpty() ? QString() : QLatin1Char(' ') + unit);
}

double WfmViewerViewModel::deltaTime() const
{
    return std::abs(m_cursor2 - m_cursor1);
}

QString WfmViewerViewModel::formatTime(double seconds)
{
    const double magnitude = std::abs(seconds);
    double scale = 1;
    QString unit = QStringLiteral("s");
    if (magnitude > 0 && magnitude < 1) {
        if (magnitude >= 1e-3) { scale = 1e3; unit = QStringLiteral("ms"); }
        else if (magnitude >= 1e-6) { scale = 1e6; unit = QStringLiteral("µs"); }
        else if (magnitude >= 1e-9) { scale = 1e9; unit = QStringLiteral("ns"); }
        else { scale = 1e12; unit = QStringLiteral("ps"); }
    }
    return QString::number(seconds * scale, 'g', 10) + QLatin1Char(' ') + unit;
}

QString WfmViewerViewModel::defaultExportPath() const
{
    const QFileInfo source(m_trace.sourcePath);
    return source.dir().filePath(source.completeBaseName() + QStringLiteral(".csv"));
}

void WfmViewerViewModel::exportCsv(const QString &path)
{
    if (path.isEmpty() || !hasTrace()) return;
    const auto trace = m_trace;
    start(Operation::Export, [path, trace](const Cancel &cancel) { return Waveform::exportCsv(path, trace, cancel); });
}

void WfmViewerViewModel::cancel()
{
    if (!m_busy) return;
    m_cancel->store(true);
    m_status = tr("Cancelling…");
    emit changed();
}
