#include "waveformplot.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <algorithm>
#include <cmath>

WaveformPlot::WaveformPlot(WfmViewerViewModel *viewModel, QWidget *parent)
    : QWidget(parent), m_viewModel(viewModel)
{
    setObjectName("waveformPlot");
    setMinimumSize(320, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setInteractionMode(InteractionMode::ZoomRegion);
    connect(viewModel, &WfmViewerViewModel::traceChanged, this, [this] {
        m_connectBounds = false; m_dragging = false; m_lastCursorClick.invalidate(); update();
    });
    connect(viewModel, &WfmViewerViewModel::viewChanged, this, qOverload<>(&WaveformPlot::update));
    connect(viewModel, &WfmViewerViewModel::cursorsChanged, this, qOverload<>(&WaveformPlot::update));
    connect(viewModel, &WfmViewerViewModel::changed, this, [this] {
        if (m_viewModel->busy()) {
            m_dragging = false; m_lastCursorClick.invalidate();
        }
        update();
    });
}

void WaveformPlot::setInteractionMode(InteractionMode mode)
{
    m_mode = mode; m_dragging = false;
    m_lastCursorClick.invalidate();
    setCursor(mode == InteractionMode::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    update();
}

QRectF WaveformPlot::plotArea() const
{
    return QRectF(86, 32, std::max(1, width() - 112), std::max(1, height() - 88));
}

void WaveformPlot::setConnectBounds(bool enabled)
{
    m_connectBounds = enabled; update();
}

int WaveformPlot::cursorNear(QPointF position, CursorAxis axis) const
{
    const bool time = axis == CursorAxis::Time;
    const auto area = plotArea();
    const auto &range = m_viewModel->viewRange();
    const double low = time ? range.from : range.minimum, high = time ? range.to : range.maximum;
    const double values[] = {
        time ? m_viewModel->cursor1() : m_viewModel->amplitudeCursor1(),
        time ? m_viewModel->cursor2() : m_viewModel->amplitudeCursor2()
    };
    int selected = 0;
    double nearest = 8; // Logical pixels, independent of the displayed units and zoom.
    for (int i = 0; i < 2; ++i) {
        if (values[i] < low || values[i] > high) continue;
        const double fraction = double((static_cast<long double>(values[i]) - low) /
                                       (static_cast<long double>(high) - low));
        const double pixel = time ? area.left() + fraction * area.width() : area.bottom() - fraction * area.height();
        const double distance = std::abs(pixel - (time ? position.x() : position.y()));
        if (distance <= 8 && (!selected || distance < nearest)) {
            selected = i + 1; nearest = distance;
        }
    }
    return selected;
}

QPointF WaveformPlot::boundedPoint(QPointF point) const
{
    const auto area = plotArea();
    return {std::clamp(point.x(), area.left(), area.right()), std::clamp(point.y(), area.top(), area.bottom())};
}

QPointF WaveformPlot::valuesAt(QPointF point) const
{
    point = boundedPoint(point);
    const auto area = plotArea();
    const auto &r = m_viewModel->viewRange();
    return {double(r.from + (static_cast<long double>(r.to) - r.from) * (point.x() - area.left()) / area.width()),
            double(r.minimum + (static_cast<long double>(r.maximum) - r.minimum) * (area.bottom() - point.y()) / area.height())};
}

QPolygonF WaveformPlot::envelope(const Waveform::Trace &trace, double from, double to, int pixels)
{
    QPolygonF result;
    if (trace.samples.isEmpty() || pixels < 1 || to <= from || to < trace.start || from > trace.end()) return result;
    const auto count = trace.pointCount();
    const auto index = [&](double time, bool upper) {
        const long double value = (static_cast<long double>(time) - trace.start) / trace.pointInterval();
        return qsizetype(std::clamp(upper ? std::ceil(value) + 1 : std::floor(value), 0.L, static_cast<long double>(count)));
    };
    const auto first = index(from, false), last = index(to, true);
    const qsizetype bucket = std::max(qsizetype(1), (last - first + pixels - 1) / pixels);
    if (trace.minMaxPairs) {
        // Paired bounds: the record does not encode which extreme occurred first
        // within each interval. Keep both bounds when decimating.
        for (qsizetype start = first; start < last; start += bucket) {
            const auto stop = std::min(last, start + bucket);
            double low = trace.samples[start*2], high = trace.samples[start*2+1];
            for (qsizetype i = start+1; i < stop; ++i) {
                low = std::min(low, trace.samples[i*2]);
                high = std::max(high, trace.samples[i*2+1]);
            }
            const double time = trace.timeAt(start*2);
            result.append(QPointF(time, low)); result.append(QPointF(time, high));
        }
        return result;
    }
    result.reserve(int(std::min(last - first, qsizetype(pixels) * 2 + 2)));
    if (first < last) result.append(QPointF(trace.timeAt(first), trace.samples[first]));
    for (qsizetype start = first; start < last; start += bucket) {
        const auto stop = std::min(last, start + bucket);
        qsizetype low = start, high = start;
        for (qsizetype i = start + 1; i < stop; ++i) {
            if (trace.samples[i] < trace.samples[low]) low = i;
            if (trace.samples[i] > trace.samples[high]) high = i;
        }
        if (low > high) std::swap(low, high);
        const QPointF lower(trace.timeAt(low), trace.samples[low]);
        if (result.isEmpty() || result.last() != lower) result.append(lower);
        if (high != low) result.append(QPointF(trace.timeAt(high), trace.samples[high]));
    }
    if (first < last) {
        const QPointF tail(trace.timeAt(last - 1), trace.samples[last - 1]);
        if (result.last() != tail) result.append(tail);
    }
    return result;
}

void WaveformPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#152233"));
    p.setRenderHint(QPainter::Antialiasing);
    const auto area = plotArea();
    p.setPen(QColor("#60718a")); p.drawRect(area);
    if (!m_viewModel->hasTrace()) {
        p.setPen(QColor("#c2cedc"));
        p.drawText(area, Qt::AlignCenter, tr("Open a WFM file to view its waveform"));
        return;
    }
    const auto &trace = m_viewModel->trace();
    const auto &r = m_viewModel->viewRange();
    const long double duration = static_cast<long double>(r.to) - r.from;
    const long double amplitude = static_cast<long double>(r.maximum) - r.minimum;
    auto point = [&](QPointF value) {
        return QPointF(area.left() + double((static_cast<long double>(value.x()) - r.from) / duration) * area.width(),
                       area.bottom() - double((static_cast<long double>(value.y()) - r.minimum) / amplitude) * area.height());
    };
    for (int i = 0; i <= 5; ++i) {
        const double fraction = i / 5.;
        const double x = area.left() + fraction * area.width(), y = area.bottom() - fraction * area.height();
        p.setPen(QColor("#2d3d52"));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        p.setPen(QColor("#c2cedc"));
        const double labelLeft = i == 0 ? x : i == 5 ? x - 110 : x - 55;
        const auto alignment = i == 0 ? Qt::AlignLeft : i == 5 ? Qt::AlignRight : Qt::AlignHCenter;
        p.drawText(QRectF(labelLeft, area.bottom() + 6, 110, 20), alignment | Qt::AlignVCenter,
                   QString::number(double(r.from + fraction * duration), 'g', 4));
        p.drawText(QRectF(0, y - 10, area.left() - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(double(r.minimum + fraction * amplitude), 'g', 4));
    }
    p.drawText(QRectF(8, 4, width() - 16, 22), Qt::AlignLeft, tr("Amplitude (%1)").arg(trace.unit));
    p.drawText(QRectF(area.left(), height() - 26, area.width(), 20), Qt::AlignCenter, tr("Time (s)"));
    p.save(); p.setClipRect(area);
    const double x1 = point({m_viewModel->cursor1(), 0}).x(), x2 = point({m_viewModel->cursor2(), 0}).x();
    p.fillRect(QRectF(QPointF(std::min(x1, x2), area.top()), QPointF(std::max(x1, x2), area.bottom())), QColor(97, 186, 255, 18));
    auto points = envelope(trace, r.from, r.to, int(area.width()));
    for (auto &v : points) v = point(v);
    p.setPen(QPen(QColor("#65baff"), 1.5));
    if (points.size() == 1) { p.setBrush(QColor("#65baff")); p.drawEllipse(points.first(), 3, 3); }
    else if (trace.minMaxPairs) {
        // Round caps keep a zero-height Min/Max pair visible as a point.
        p.setPen(QPen(QColor("#65baff"), 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLines(points.constData(), points.size()/2);
        for (qsizetype i = 0; i + 1 < points.size(); i += 2)
            if (points[i] == points[i+1]) p.drawPoint(points[i]);
        if (m_connectBounds) {
            QPolygonF lower, upper;
            lower.reserve(points.size()/2); upper.reserve(points.size()/2);
            for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
                lower.append(points[i]); upper.append(points[i+1]);
            }
            p.drawPolyline(lower); p.drawPolyline(upper);
        }
    }
    else p.drawPolyline(points);
    for (int cursor = 1; cursor <= 2; ++cursor) {
        const double time = cursor == 1 ? m_viewModel->cursor1() : m_viewModel->cursor2();
        if (time < r.from || time > r.to) continue;
        const double x = cursor == 1 ? x1 : x2;
        const QColor color(cursor == 1 ? "#67e8d0" : "#ffc76b");
        p.setPen(QPen(color, 1.5, Qt::DashLine));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        p.setPen(color);
        const double labelX = std::clamp(x + 4, area.left(), area.right() - 28);
        p.drawText(QRectF(labelX, area.top() + (cursor == 1 ? 4 : 24), 28, 20), QStringLiteral("T%1").arg(cursor));
    }
    for (int cursor = 1; cursor <= 2; ++cursor) {
        const double value = cursor == 1 ? m_viewModel->amplitudeCursor1() : m_viewModel->amplitudeCursor2();
        if (value < r.minimum || value > r.maximum) continue;
        const double y = point({r.from, value}).y();
        const QColor color(cursor == 1 ? "#c4a1ff" : "#ff99cf");
        p.setPen(QPen(color, 1.5, Qt::DashLine));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        p.setPen(color);
        const double labelY = std::clamp(y - 22, area.top(), area.bottom() - 20);
        p.drawText(QRectF(area.right() - (cursor == 1 ? 66 : 32), labelY, 28, 20), QStringLiteral("Y%1").arg(cursor));
    }
    if (m_dragging && m_mode == InteractionMode::ZoomRegion) {
        p.setPen(QPen(QColor("#ffffff"), 1, Qt::DashLine)); p.setBrush(QColor(101, 186, 255, 40));
        p.drawRect(QRectF(m_start, m_current).normalized());
    }
    p.restore();
}

void WaveformPlot::wheelEvent(QWheelEvent *event)
{
    if (!isEnabled() || m_viewModel->busy() || !m_viewModel->hasTrace() || m_dragging ||
        !plotArea().contains(event->position()) || event->angleDelta().y() == 0) return;
    const auto anchor = valuesAt(event->position());
    m_viewModel->zoom(event->angleDelta().y() > 0 ? .8 : 1.25, anchor.x(), anchor.y());
    event->accept();
}

void WaveformPlot::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled() || m_viewModel->busy() || !m_viewModel->hasTrace() ||
        event->button() != Qt::LeftButton || !plotArea().contains(event->position())) return;
    m_dragging = true; m_start = m_current = boundedPoint(event->position());
    m_lastCursorClick.invalidate();
    if (m_mode == InteractionMode::Cursors) {
        m_activeTimeCursor = cursorNear(m_current, CursorAxis::Time);
        m_activeAmplitudeCursor = cursorNear(m_current, CursorAxis::Amplitude);
        if (!m_activeTimeCursor && !m_activeAmplitudeCursor) { m_dragging = false; return; }
        m_beforeClick1 = m_viewModel->cursor1(); m_beforeClick2 = m_viewModel->cursor2();
        m_beforeAmplitude1 = m_viewModel->amplitudeCursor1(); m_beforeAmplitude2 = m_viewModel->amplitudeCursor2();
        moveGesture(m_current);
    }
    if (m_mode == InteractionMode::Pan) setCursor(Qt::ClosedHandCursor);
    update(); event->accept();
}

void WaveformPlot::moveGesture(QPointF position)
{
    const auto next = boundedPoint(position);
    if (m_mode == InteractionMode::Cursors) {
        const auto values = valuesAt(next);
        if (m_activeTimeCursor) m_viewModel->setCursor(m_activeTimeCursor, values.x());
        if (m_activeAmplitudeCursor) m_viewModel->setAmplitudeCursor(m_activeAmplitudeCursor, values.y());
    }
    else if (m_mode == InteractionMode::Pan) {
        const auto previousValue = valuesAt(m_current), nextValue = valuesAt(next);
        m_viewModel->pan(previousValue.x() - nextValue.x(), previousValue.y() - nextValue.y());
    }
    m_current = next; update();
}

void WaveformPlot::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && !m_viewModel->busy()) moveGesture(event->position());
}

void WaveformPlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) return;
    moveGesture(event->position());
    const QRectF selection = QRectF(m_start, m_current).normalized();
    m_dragging = false;
    if (m_mode == InteractionMode::Cursors &&
        (m_current - m_start).manhattanLength() <= QApplication::startDragDistance()) {
        m_lastCursorClick.start(); m_lastClickPoint = m_current;
    }
    if (m_mode == InteractionMode::ZoomRegion && selection.width() >= 6 && selection.height() >= 6) {
        const auto low = valuesAt(selection.bottomLeft()), high = valuesAt(selection.topRight());
        m_viewModel->zoomTo({low.x(), high.x(), low.y(), high.y()});
    }
    setCursor(m_mode == InteractionMode::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    update(); event->accept();
}

void WaveformPlot::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!isEnabled() || m_viewModel->busy() || !m_viewModel->hasTrace()) return;
    if (event->button() == Qt::LeftButton && plotArea().contains(event->position())) {
        // The first click of a real double-click already moved a cursor. Undo that
        // click so fitting the view preserves both absolute cursor times.
        if (m_mode == InteractionMode::Cursors && m_lastCursorClick.isValid() &&
            m_lastCursorClick.elapsed() <= QApplication::doubleClickInterval() &&
            (event->position() - m_lastClickPoint).manhattanLength() <= QApplication::startDragDistance()) {
            m_viewModel->setCursor(1, m_beforeClick1); m_viewModel->setCursor(2, m_beforeClick2);
            m_viewModel->setAmplitudeCursor(1, m_beforeAmplitude1); m_viewModel->setAmplitudeCursor(2, m_beforeAmplitude2);
        }
        m_lastCursorClick.invalidate();
        m_dragging = false; m_viewModel->fit(); update();
    }
}
