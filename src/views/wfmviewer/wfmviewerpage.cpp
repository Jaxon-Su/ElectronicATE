#include "wfmviewerpage.h"
#include "waveformplot.h"
#include "../../viewmodels/wfmviewer/wfmviewerviewmodel.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QGridLayout>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

WfmViewerPage::WfmViewerPage(WfmViewerViewModel *viewModel, QWidget *parent)
    : QWidget(parent), m_viewModel(viewModel)
{
    setObjectName("wfmViewerPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,6,8,6);
    layout->setSpacing(4);
    auto button = [this](const QString &text, const char *name) {
        auto *result = new QPushButton(text, this); result->setObjectName(name); return result;
    };
    auto *toolbar = new QHBoxLayout;
    m_open = button(tr("Open WFM…"), "openWfm");
    m_export = button(tr("Export CSV…"), "exportCsv");
    m_export->setToolTip(tr("Export the full original waveform, independent of the view and cursors."));
    m_cancel = button(tr("Cancel"), "cancelWfm");
    toolbar->addWidget(m_open); toolbar->addWidget(m_export); toolbar->addWidget(m_cancel);
    m_source = new QLineEdit(this); m_source->setObjectName("waveformSource");
    m_source->setReadOnly(true); m_source->setFrame(false); m_source->setAlignment(Qt::AlignRight);
    m_source->setAccessibleName(tr("Waveform file path"));
    m_source->setStyleSheet("QLineEdit { background: transparent; border: none; }");
    m_source->setMinimumWidth(0);
    m_source->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    toolbar->addWidget(m_source, 1); layout->addLayout(toolbar);
    auto *navigation = new QHBoxLayout;
    m_modes = new QButtonGroup(this);
    const auto modeButton = [&](const QString &text, const char *name, WaveformPlot::InteractionMode mode) {
        auto *result = button(text, name); result->setCheckable(true);
        m_modes->addButton(result, static_cast<int>(mode)); navigation->addWidget(result);
        return result;
    };
    modeButton(tr("Cursor"), "cursorMode", WaveformPlot::InteractionMode::Cursors);
    modeButton(tr("Pan"), "panMode", WaveformPlot::InteractionMode::Pan);
    modeButton(tr("Zoom"), "zoomMode", WaveformPlot::InteractionMode::ZoomRegion)->setChecked(true);
    m_fit = button(tr("Fit"), "fitWaveform");
    navigation->addWidget(m_fit);
    m_connectBounds = new QCheckBox(tr("上下界連線"), this);
    m_connectBounds->setObjectName("connectBounds");
    m_connectBounds->setToolTip(tr("Connect adjacent Min/Max bounds for display only. Values inside each interval are not reconstructed."));
    navigation->addWidget(m_connectBounds);
    m_status = new QLabel(this); m_status->setObjectName("waveformStatus");
    m_status->setTextFormat(Qt::PlainText); m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    navigation->addWidget(m_status, 1);
    layout->addLayout(navigation);
    m_plot = new WaveformPlot(viewModel, this); layout->addWidget(m_plot, 1);
    m_readouts = new QGridLayout;
    m_readouts->setHorizontalSpacing(12); m_readouts->setVerticalSpacing(2);
    auto readout = [this](const char *name, const char *color) {
        auto *label = new QLabel(this); label->setObjectName(name); label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setStyleSheet(QStringLiteral("font-weight: bold; color: %1;").arg(color));
        // Let resizeEvent choose a narrower grid before enforcing any text width.
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        return label;
    };
    m_cursor1 = readout("cursor1Readout", "#087f70"); m_cursor2 = readout("cursor2Readout", "#986014");
    m_delta = readout("deltaTimeReadout", "#184f91");
    m_amplitude1 = readout("amplitudeCursor1Readout", "#7044a1");
    m_amplitude2 = readout("amplitudeCursor2Readout", "#a53070");
    m_deltaAmplitude = readout("deltaAmplitudeReadout", "#184f91");
    layout->addLayout(m_readouts);
    m_stats = new QLabel(this); m_stats->setObjectName("waveformStatistics");
    m_stats->setTextFormat(Qt::PlainText); m_stats->setWordWrap(true);
    m_stats->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_stats);
    m_error = new QLabel(this); m_error->setTextFormat(Qt::PlainText); m_error->setStyleSheet("color: #b42318;");
    m_error->setWordWrap(true); layout->addWidget(m_error);
    connect(m_open, &QPushButton::clicked, this, [this] {
        m_viewModel->load(QFileDialog::getOpenFileName(this, tr("Open waveform"), {}, tr("Tektronix waveform (*.wfm);;All files (*)")));
    });
    connect(m_export, &QPushButton::clicked, this, [this] {
        QFileDialog dialog(this, tr("Export full waveform to CSV"), m_viewModel->defaultExportPath(), tr("CSV (*.csv)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave); dialog.setDefaultSuffix("csv");
        if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
            m_viewModel->exportCsv(dialog.selectedFiles().first());
    });
    connect(m_cancel, &QPushButton::clicked, viewModel, &WfmViewerViewModel::cancel);
    connect(m_fit, &QPushButton::clicked, viewModel, &WfmViewerViewModel::fit);
    connect(m_connectBounds, &QCheckBox::toggled, m_plot, &WaveformPlot::setConnectBounds);
    const auto updateInteraction = [this] {
        const auto mode = static_cast<WaveformPlot::InteractionMode>(m_modes->checkedId());
        m_plot->setInteractionMode(mode);
        QString hint;
        if (mode == WaveformPlot::InteractionMode::ZoomRegion) hint = tr("Drag a rectangle to zoom time and amplitude.");
        else if (mode == WaveformPlot::InteractionMode::Pan) hint = tr("Drag to pan time and amplitude.");
        else hint = tr("Drag near T1 / T2 or Y1 / Y2. Drag their intersection to move both axes. Δt = |T2 − T1|; ΔY = |Y2 − Y1|.");
        hint += tr("  Wheel: zoom · Double-click: fit");
        m_plot->setToolTip(hint);
        m_modes->checkedButton()->setToolTip(hint);
        refresh();
    };
    connect(m_modes, &QButtonGroup::idClicked, this, updateInteraction); updateInteraction();
    connect(viewModel, &WfmViewerViewModel::changed, this, &WfmViewerPage::refresh);
    connect(viewModel, &WfmViewerViewModel::traceChanged, this, &WfmViewerPage::refreshTrace);
    connect(viewModel, &WfmViewerViewModel::viewChanged, this, &WfmViewerPage::refreshCursors);
    connect(viewModel, &WfmViewerViewModel::cursorsChanged, this, &WfmViewerPage::refreshCursors);
    refreshTrace(); refresh();
}

void WfmViewerPage::refresh()
{
    const bool idle = !m_viewModel->busy(), ready = idle && m_viewModel->hasTrace();
    m_open->setEnabled(idle); m_export->setEnabled(ready); m_cancel->setEnabled(!idle);
    for (auto *button : m_modes->buttons()) button->setEnabled(ready);
    m_plot->setEnabled(ready);
    m_fit->setEnabled(ready);
    m_connectBounds->setEnabled(ready && m_viewModel->trace().minMaxPairs);
    m_status->setText(m_viewModel->status()); m_error->setText(m_viewModel->error());
    m_status->setToolTip(m_viewModel->status());
    m_error->setVisible(!m_viewModel->error().isEmpty());
}

void WfmViewerPage::refreshTrace()
{
    m_connectBounds->setChecked(false);
    const auto &t = m_viewModel->trace();
    m_source->setText(m_viewModel->hasTrace() ? t.sourcePath : tr("No waveform loaded"));
    m_source->setToolTip(m_source->text()); m_source->setCursorPosition(m_source->text().size());
    m_stats->clear(); m_stats->setVisible(m_viewModel->hasTrace());
    if (m_viewModel->hasTrace()) {
        const auto value = [&](double v) { return WfmViewerViewModel::formatAmplitude(v, t.unit); };
        m_stats->setText((t.minMaxPairs
            ? tr("Min/Max waveform · Pairs: %1 · Pair interval: %2 · Max: %3 · Min: %4")
            : tr("Full waveform · Points: %1 · Sample interval: %2 · Max: %3 · Min: %4"))
            .arg(t.pointCount()).arg(WfmViewerViewModel::formatTime(t.pointInterval()))
            .arg(value(t.statistics.maximum), value(t.statistics.minimum)));
        m_stats->setToolTip(t.minMaxPairs
            ? tr("Each pair stores the interval's minimum and maximum. Their exact times within that interval are unknown. CSV exports Time / Min / Max.")
            : QString());
    }
    refreshCursors();
}

void WfmViewerPage::refreshCursors()
{
    const auto &range = m_viewModel->viewRange();
    auto show = [&](QLabel *label, const QString &name, double value, bool cursor) {
        const bool outside = cursor && (value < range.from || value > range.to);
        label->setText(name + (m_viewModel->hasTrace() ? WfmViewerViewModel::formatTime(value) +
            (outside ? tr(" (outside view)") : QString()) : QStringLiteral("—")));
        label->setToolTip(m_viewModel->hasTrace() ? QString::number(value, 'g', 17) + QStringLiteral(" s") : QString());
    };
    show(m_cursor1, QStringLiteral("T1: "), m_viewModel->cursor1(), true);
    show(m_cursor2, QStringLiteral("T2: "), m_viewModel->cursor2(), true);
    show(m_delta, QStringLiteral("Δt: "), m_viewModel->deltaTime(), false);
    const auto showAmplitude = [&](QLabel *label, const QString &name, long double value, bool cursor) {
        const bool outside = cursor && (value < range.minimum || value > range.maximum);
        const auto &unit = m_viewModel->trace().unit;
        label->setText(name + (m_viewModel->hasTrace() ? WfmViewerViewModel::formatAmplitude(value, unit) +
            (outside ? tr(" (outside view)") : QString()) : QStringLiteral("—")));
        label->setToolTip(m_viewModel->hasTrace() ? WfmViewerViewModel::formatAmplitude(value, unit, 17) : QString());
    };
    showAmplitude(m_amplitude1, QStringLiteral("Y1: "), m_viewModel->amplitudeCursor1(), true);
    showAmplitude(m_amplitude2, QStringLiteral("Y2: "), m_viewModel->amplitudeCursor2(), true);
    showAmplitude(m_deltaAmplitude, QStringLiteral("ΔY: "), m_viewModel->deltaAmplitude(), false);
    arrangeReadouts();
}

void WfmViewerPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrangeReadouts();
}

void WfmViewerPage::arrangeReadouts()
{
    if (!m_readouts) return;
    const std::array<QLabel *, 6> labels{m_cursor1, m_cursor2, m_delta, m_amplitude1, m_amplitude2, m_deltaAmplitude};
    int requiredWidth = 0;
    for (auto *label : labels) requiredWidth = std::max(requiredWidth, label->sizeHint().width());
    const auto margins = layout()->contentsMargins();
    const int available = width() - margins.left() - margins.right();
    int columns = 1;
    for (int candidate : {6, 3, 2}) {
        if (candidate * requiredWidth + (candidate - 1) * m_readouts->horizontalSpacing() <= available) {
            columns = candidate; break;
        }
    }
    if (columns == m_readoutColumns) return;
    for (auto *label : labels) m_readouts->removeWidget(label);
    for (int column = 0; column < 6; ++column) m_readouts->setColumnStretch(column, column < columns ? 1 : 0);
    for (int i = 0; i < 6; ++i) m_readouts->addWidget(labels[i], i / columns, i % columns);
    m_readoutColumns = columns;
}
