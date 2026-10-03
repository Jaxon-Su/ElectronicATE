#pragma once
#include "../../infrastructure/storage/waveformocrfiles.h"
#include "../../service/waveformocr/ollamaclient.h"
#include "ocrresultsmodel.h"
#include <QFutureWatcher>

class Page6ViewModel : public QObject
{
    Q_OBJECT
  public:
    explicit Page6ViewModel(QObject *parent = nullptr);
    ~Page6ViewModel() override;
    OcrResultsModel *results() { return &m_results; }
    bool busy() const { return m_state != State::Idle; }
    bool hasUnexportedResults() const { return m_dirty; }
    QString message() const { return m_message; }
    int completed() const { return m_completed; }
    int total() const { return m_paths.size(); }
    void refreshModels(const QUrl &endpoint);
    void start(const QString &folder, const QUrl &endpoint, const QString &model);
    void stop();
    void exportResults(const QString &path);
  signals:
    void changed();
    void modelNamesChanged(const QStringList &names);
    void imageStarted(const QString &path);
    void exportFinished(const QString &path);

  private:
    enum class State
    {
        Idle,
        Models,
        Scanning,
        Recognizing,
        Exporting
    };
    void nextImage();
    void setState(State state, const QString &message);
    State m_state = State::Idle;
    OcrResultsModel m_results;
    OllamaClient m_client;
    QFutureWatcher<WaveformOcr::ScanResult> m_scan;
    QFutureWatcher<WaveformOcr::ExportResult> m_export;
    WaveformOcr::Cancellation m_cancel;
    QString m_root, m_model, m_message = "選擇圖片資料夾，連線本機 Ollama 後開始辨識。";
    QUrl m_endpoint;
    QStringList m_paths;
    int m_completed = 0, m_failed = 0;
    bool m_dirty = false;
};
