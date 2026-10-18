#include "waveformocrviewmodel.h"
#include "../../service/waveformocr/waveformocrparser.h"
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrentRun>

WaveformOcrViewModel::WaveformOcrViewModel(QObject *parent) : QObject(parent)
{
    connect(&m_results, &OcrResultsModel::resultEdited, this,
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
                const auto run = m_runId;
                emit modelNamesChanged(names);
                if (m_state != State::Models || run != m_runId)
                    return;
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
                m_resultRoot = m_root;
                m_completed = 0;
                m_failed = 0;
                m_partial = 0;
                m_results.clear();
                m_dirty = false;
                if (m_cancel->load())
                {
                    setState(State::Idle, "已停止；尚未送出圖片。");
                    return;
                }
                setState(State::Recognizing, "準備辨識");
                nextImage();
            });
    connect(&m_client, &OllamaClient::recognitionReady, this,
            [this](const QString &text, const QString &error)
            {
                if (m_state != State::Recognizing)
                    return;
                const auto run = m_runId;
                QVector<WaveformOcr::Row> rows;
                if (error.isEmpty())
                    rows = WaveformOcr::parseText(m_paths[m_completed], m_root, text);
                else
                {
                    auto row = WaveformOcr::failedRow(m_paths[m_completed], m_root, error);
                    row.rawResponse = text;
                    rows << row;
                }
                bool any = false, complete = !rows.isEmpty();
                for (const auto &row : rows)
                {
                    complete &= row.error.isEmpty();
                    for (const auto &value : row.values)
                    {
                        any |= value.number.has_value();
                        complete &= value.number.has_value();
                    }
                }
                if (!any)
                    ++m_failed;
                else if (!complete)
                    ++m_partial;
                ++m_completed;
                m_results.append(rows);
                if (run != m_runId)
                    return;
                emit changed();
                // Also handles synchronous validation failures without recursion or starving Stop.
                QTimer::singleShot(0, this, [this, run] {
                    if (run == m_runId)
                        nextImage();
                });
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
WaveformOcrViewModel::~WaveformOcrViewModel()
{
    if (m_cancel)
        m_cancel->store(true);
    m_client.cancel();
    // Workers capture only snapshots and shared cancellation, never this or widgets.
    m_scan.waitForFinished();
    m_export.waitForFinished();
}
void WaveformOcrViewModel::setState(State state, const QString &message)
{
    m_state = state;
    m_message = message;
    m_results.setLocked(busy());
    emit changed();
}
OcrSelection WaveformOcrViewModel::selection(int index) const
{
    const auto *row = m_results.row(index);
    if (!row)
        return {};
    const auto issues = WaveformOcr::issues(*row);
    const QString details = "來源: " + QDir::toNativeSeparators(row->imagePath) +
        "\n通道（選填）: " + (row->channel.isEmpty() ? "未提供" : row->channel) +
        "\n統計範圍（選填）: " +
        (row->scope.isEmpty() || row->scope == "unspecified" ? "未提供" : row->scope) +
        "\n狀態: " + WaveformOcr::status(*row) +
        "\n\n數值問題:\n" + (issues.isEmpty() ? "無" : issues.join("\n")) +
        "\n\n辨識原值:\n" + row->originalValues +
        "\n\n原始辨識訊息:\n" + row->error + "\n\n備註:\n" + row->note;
    return {row->imagePath, details, row->rawResponse};
}
QString WaveformOcrViewModel::suggestedExportPath() const
{
    return QDir(m_resultRoot).filePath("WaveformOCR_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".xlsx");
}
void WaveformOcrViewModel::refreshModels()
{
    refreshModels(QUrl(QStringLiteral("http://127.0.0.1:11434")));
}
void WaveformOcrViewModel::start(const QString &folder, const QString &model)
{
    start(folder, QUrl(QStringLiteral("http://127.0.0.1:11434")), model);
}
void WaveformOcrViewModel::refreshModels(const QUrl &endpoint)
{
    if (busy())
        return;
    const auto run = ++m_runId;
    setState(State::Models, "正在連線 Ollama…");
    if (m_state == State::Models && run == m_runId)
        m_client.listModels(endpoint);
}
void WaveformOcrViewModel::start(const QString &folder, const QUrl &endpoint, const QString &model)
{
    if (busy())
        return;
    const auto error = OllamaClient::validateEndpoint(endpoint);
    if (!error.isEmpty() || model.trimmed().isEmpty() || !QFileInfo(folder).isDir())
    {
        setState(State::Idle, !error.isEmpty() ? error : "請選擇有效資料夾並填寫視覺模型名稱");
        return;
    }
    ++m_runId;
    m_root = QFileInfo(folder).absoluteFilePath();
    m_endpoint = endpoint;
    m_model = model;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    setState(State::Scanning, "正在掃描圖片…");
    const auto root = m_root;
    const auto cancel = m_cancel;
    m_scan.setFuture(QtConcurrent::run([root, cancel] { return WaveformOcr::scanImages(root, cancel); }));
}
void WaveformOcrViewModel::nextImage()
{
    if (m_state != State::Recognizing)
        return;
    if (m_completed >= m_paths.size())
    {
        setState(
            State::Idle,
            QString("批次完成：%1 張；原始辨識 %2 張四值齊全、%3 張部分缺值、%4 張無有效數值。可修改後匯出。")
                .arg(m_completed).arg(m_completed - m_failed - m_partial).arg(m_partial).arg(m_failed));
        return;
    }
    const auto run = m_runId;
    const auto path = m_paths[m_completed];
    m_message = QString("辨識 %1 / %2：%3")
                    .arg(m_completed + 1)
                    .arg(m_paths.size())
                    .arg(QFileInfo(path).fileName());
    emit changed();
    if (m_state != State::Recognizing || run != m_runId)
        return;
    emit imageStarted(path);
    if (m_state == State::Recognizing && run == m_runId)
        m_client.recognize(m_endpoint, m_model, path);
}
void WaveformOcrViewModel::stop()
{
    if (!busy())
        return;
    ++m_runId;
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
void WaveformOcrViewModel::exportResults(const QString &path)
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
