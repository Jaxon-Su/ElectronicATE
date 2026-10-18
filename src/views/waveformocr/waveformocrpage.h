#pragma once
#include <QWidget>
class WaveformOcrViewModel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTableView;
class QGraphicsView;
class QGraphicsScene;
class QPlainTextEdit;

class WaveformOcrPage : public QWidget
{
    Q_OBJECT
  public:
    explicit WaveformOcrPage(WaveformOcrViewModel *viewModel, QWidget *parent = nullptr);
    void finishEditing();

  private:
    void updateState();
    void showSelection();
    void preview(const QString &path);
    void fitImage();
    WaveformOcrViewModel *m_vm;
    QLineEdit *m_folder;
    QComboBox *m_models;
    QPushButton *m_browse, *m_connect, *m_start, *m_stop, *m_export, *m_edit;
    QLabel *m_status, *m_imagePath;
    QProgressBar *m_progress;
    QTableView *m_table;
    QGraphicsView *m_preview;
    QGraphicsScene *m_scene;
    QPlainTextEdit *m_details, *m_raw;
    QString m_currentPath;
};
