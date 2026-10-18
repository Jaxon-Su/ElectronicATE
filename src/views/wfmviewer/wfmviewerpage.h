#pragma once
#include <QWidget>
class WfmViewerViewModel;
class WaveformPlot;
class QPushButton;
class QLabel;
class QButtonGroup;
class QCheckBox;
class QLineEdit;
class QGridLayout;

class WfmViewerPage : public QWidget {
    Q_OBJECT
public:
    explicit WfmViewerPage(WfmViewerViewModel *viewModel, QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void refresh();
    void refreshTrace();
    void refreshCursors();
    void arrangeReadouts();
    WfmViewerViewModel *m_viewModel;
    WaveformPlot *m_plot;
    QButtonGroup *m_modes;
    QCheckBox *m_connectBounds;
    QPushButton *m_open, *m_export, *m_cancel, *m_fit;
    QLineEdit *m_source;
    QLabel *m_status, *m_error, *m_cursor1, *m_cursor2, *m_delta;
    QLabel *m_amplitude1, *m_amplitude2, *m_deltaAmplitude;
    QLabel *m_stats;
    QGridLayout *m_readouts = nullptr;
    int m_readoutColumns = 0;
};
