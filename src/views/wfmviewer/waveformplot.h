#pragma once
#include "../../viewmodels/wfmviewer/wfmviewerviewmodel.h"
#include <QWidget>
#include <QPolygonF>
#include <QElapsedTimer>

class WaveformPlot : public QWidget {
    Q_OBJECT
public:
    enum class InteractionMode { ZoomRegion, Pan, Cursors };
    explicit WaveformPlot(WfmViewerViewModel *viewModel, QWidget *parent = nullptr);
    void setInteractionMode(InteractionMode mode);
    void setConnectBounds(bool enabled);
    QRectF plotArea() const;
    // Min/max envelope in time/value coordinates; keeps spikes when many samples occupy a pixel.
    static QPolygonF envelope(const Waveform::Trace &trace, double from, double to, int pixels);
protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
    enum class CursorAxis { Time, Amplitude };
    int cursorNear(QPointF position, CursorAxis axis) const;
    QPointF boundedPoint(QPointF point) const;
    QPointF valuesAt(QPointF point) const;
    void moveGesture(QPointF position);
    WfmViewerViewModel *m_viewModel;
    InteractionMode m_mode = InteractionMode::ZoomRegion;
    bool m_dragging = false;
    bool m_connectBounds = false;
    int m_activeTimeCursor = 0, m_activeAmplitudeCursor = 0;
    QPointF m_start, m_current;
    QElapsedTimer m_lastCursorClick;
    QPointF m_lastClickPoint;
    double m_beforeClick1 = 0, m_beforeClick2 = 0;
    double m_beforeAmplitude1 = 0, m_beforeAmplitude2 = 0;
};
