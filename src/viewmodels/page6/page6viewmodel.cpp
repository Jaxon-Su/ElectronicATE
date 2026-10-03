#include "page6viewmodel.h"
#include "../../service/waveformocr/waveformocrparser.h"
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrentRun>

Page6ViewModel::Page6ViewModel(QObject *parent) : QObject(parent)
{
    connect(&m_results, &QAbstractItemModel::dataChanged, this,
            [this]
            {
                m_dirty = true;
                emit changed();
            });
    connect(&m_results, &QAbstractItemModel::rowsInserted, this,
            [this]
            {
                m_dirty = true;
                emit changed();
            });
    connect(&m_client, &OllamaClient::modelsReady, this,
            [this](const QStringList &names, const QString &error)
            {
                if (m_state != State::Models)
                    return;
                emit modelNamesChanged(names);
                setState(State::Idle, error.isEmpty()
                                          ? QString("Ollama 已連線，可用模型 %1 個").arg(names.size())
                                          : "連線失敗: " + error);
            });
    connect(&m_scan, &QFutureWatcher<WaveformOcr::ScanResult>::finished, this,
            [this]
            {
                const auto result = m_scan.result();
                if (m_cancel->load() || !result.error.isEmpty())
                {
                    setState(State::Idle, m_cancel->load() ? "已停止掃描；保留原有結果。" : result.error);
                    return;
                }
                m_paths = result.paths;
                m_completed = 0;
                m_failed = 0;
                m_results.clear();
                m_dirty = false;
                setState(State::Recognizing, "準備辨識");
                nextImage();
            });
    connect(&m_client, &OllamaClient::recognitionReady, this,
            [this](const QString &text, const QString &error)
            {
                if (m_state != State::Recognizing)
                    return;
                QVector<WaveformOcr::Row> rows;
                if (error.isEmpty())
                    rows = WaveformOcr::parseText(m_paths[m_completed], m_root, text);
                else
                {
                    auto row = WaveformOcr::failedRow(m_paths[m_completed], m_root, error);
                    row.rawResponse = text;
                    rows << row;
                }
                bool failed = false;
                for (const auto &row : rows)
                    failed |= !row.error.isEmpty();
                if (failed)
                    ++m_failed;
                m_results.append(rows);
                ++m_completed;
                emit changed();
                // Also handles synchronous validation failures without recursion or starving Stop.
                QTimer::singleShot(0, this, &Page6ViewModel::nextImage);
            });
    connect(&m_export, &QFutureWatcher<WaveformOcr::ExportResult>::finished, this,
            [this]
            {
                const auto result = m_export.result();
                if (result.error.isEmpty())
                {
                    m_dirty = false;
                    setState(State::Idle, "已匯出: " + QDir::toNativeSeparators(result.path));
                    emit exportFinished(result.path);
                }
                else
                    setState(State::Idle, "匯出未完成: " + result.error);
            });
}
Page6ViewModel::~Page6ViewModel()
{
    if (m_cancel)
        m_cancel->store(true);
    m_client.cancel();
    // Workers capture only snapshots and shared cancellation, never this or widgets.
    m_scan.waitForFinished();
    m_export.waitForFinished();
}
void Page6ViewModel::setState(State state, const QString &message)
{
    m_state = state;
    m_message = message;
    m_results.setLocked(busy());
    emit changed();
}
void Page6ViewModel::refreshModels(const QUrl &endpoint)
{
    if (busy())
        return;
    setState(State::Models, "正在連線 Ollama…");
    m_client.listModels(endpoint);
}
void Page6ViewModel::start(const QString &folder, const QUrl &endpoint, const QString &model)
{
    if (busy())
        return;
    const auto error = OllamaClient::validateEndpoint(endpoint);
    if (!error.isEmpty() || model.trimmed().isEmpty() || !QFileInfo(folder).isDir())
    {
        setState(State::Idle, !error.isEmpty() ? error : "請選擇有效資料夾並填寫視覺模型名稱");
        return;
    }
    m_root = QFileInfo(folder).absoluteFilePath();
    m_endpoint = endpoint;
    m_model = model;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    setState(State::Scanning, "正在掃描圖片…");
    const auto root = m_root;
    const auto cancel = m_cancel;
    m_scan.setFuture(QtConcurrent::run([root, cancel] { return WaveformOcr::scanImages(root, cancel); }));
}
void Page6ViewModel::nextImage()
{
    if (m_state != State::Recognizing)
        return;
    if (m_completed >= m_paths.size())
    {
        setState(
            State::Idle,
            QString("批次完成：%1 張，%2 張辨識失敗。請對照原圖核對後匯出。").arg(m_completed).arg(m_failed));
        return;
    }
    m_message = QString("辨識 %1 / %2：%3")
                    .arg(m_completed + 1)
                    .arg(m_paths.size())
                    .arg(QFileInfo(m_paths[m_completed]).fileName());
    emit changed();
    emit imageStarted(m_paths[m_completed]);
    m_client.recognize(m_endpoint, m_model, m_paths[m_completed]);
}
void Page6ViewModel::stop()
{
    if (!busy())
        return;
    if (m_cancel)
        m_cancel->store(true);
    if (m_state == State::Scanning || m_state == State::Exporting)
    {
        m_message = "正在停止…";
        emit changed();
        return;
    }
    m_client.cancel();
    setState(State::Idle, QString("已停止；保留 %1 張已完成結果，其餘圖片未處理。").arg(m_completed));
}
void Page6ViewModel::exportResults(const QString &path)
{
    if (busy() || m_results.rows().isEmpty() || path.isEmpty())
        return;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto rows = m_results.rows();
    const auto cancel = m_cancel;
    setState(State::Exporting, "正在匯出 Excel…");
    m_export.setFuture(
        QtConcurrent::run([rows, path, cancel] { return WaveformOcr::exportExcel(rows, path, cancel); }));
}
