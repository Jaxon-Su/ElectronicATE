#pragma once

#include "../../data/waveformdata.h"
#include <QObject>
#include <QFutureWatcher>
#include <atomic>
#include <functional>
#include <memory>

struct WaveformViewRange {
    double from = 0;
    double to = 1;
    double minimum = -1;
    double maximum = 1;
};

class WfmViewerViewModel : public QObject {
    Q_OBJECT
public:
    explicit WfmViewerViewModel(QObject *parent = nullptr);
    ~WfmViewerViewModel() override;
    const Waveform::Trace &trace() const { return m_trace; }
    const WaveformViewRange &viewRange() const { return m_range; }
    double cursor1() const { return m_cursor1; }
    double cursor2() const { return m_cursor2; }
    double deltaTime() const;
    double amplitudeCursor1() const { return m_amplitude1; }
    double amplitudeCursor2() const { return m_amplitude2; }
    long double deltaAmplitude() const;
    static QString formatTime(double seconds);
    static QString formatAmplitude(long double value, const QString &unit, int precision = 10);
    bool hasTrace() const { return !m_trace.samples.isEmpty(); }
    bool busy() const { return m_busy; }
    const QString &status() const { return m_status; }
    const QString &error() const { return m_error; }
    QString defaultExportPath() const;
    void load(const QString &path);
    void fit();
    void zoomTo(const WaveformViewRange &range);
    void zoom(double factor, double timeAnchor, double valueAnchor);
    void pan(double timeDelta, double valueDelta);
    void setCursor(int cursor, double time);
    void setAmplitudeCursor(int cursor, double value);
    void exportCsv(const QString &path);
    void cancel();
signals:
    void changed();
    void traceChanged();
    void viewChanged();
    void cursorsChanged();
private:
    enum class Operation { Load, Export };
    using Cancel = std::shared_ptr<std::atomic_bool>;
    void start(Operation operation, std::function<Waveform::Result(const Cancel &)> job);
    void initializeView();
    void constrainView(long double from, long double to, long double minimum, long double maximum);
    Waveform::Trace m_trace;
    WaveformViewRange m_range, m_fullRange;
    double m_cursor1 = 0, m_cursor2 = 0;
    double m_amplitude1 = 0, m_amplitude2 = 0;
    QFutureWatcher<Waveform::Result> m_watcher;
    Cancel m_cancel;
    bool m_busy = false;
    Operation m_operation = Operation::Load;
    QString m_status, m_error;
};
