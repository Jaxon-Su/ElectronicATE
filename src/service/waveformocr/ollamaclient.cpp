#include "ollamaclient.h"
#include "waveformocrparser.h"
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <memory>

OllamaClient::OllamaClient(QObject *parent) : QObject(parent)
{
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this,
            [this]
            {
                if (m_reply)
                {
                    m_reply->setProperty("ocrTimeout", true);
                    m_reply->abort();
                }
            });
}

QString OllamaClient::validateEndpoint(const QUrl &url)
{
    const auto host = url.host().toLower();
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") ||
        (host != "127.0.0.1" && host != "localhost" && host != "::1") || !url.userInfo().isEmpty() ||
        url.hasQuery() || url.hasFragment() ||
        (!url.path().isEmpty() && url.path() != "/" && url.path() != "/api/generate"))
        return "請輸入本機 Ollama 位址，例如 http://127.0.0.1:11434";
    return {};
}

OllamaClient::~OllamaClient()
{
    cancel();
}

void OllamaClient::listModels(const QUrl &base)
{
    request(base, {}, true);
}

void OllamaClient::recognize(const QUrl &base, const QString &model, const QString &path)
{
    if (busy())
        return;
    const auto endpointError = validateEndpoint(base);
    if (!endpointError.isEmpty())
    {
        emit recognitionReady({}, endpointError);
        return;
    }
    if (model.trimmed().isEmpty())
    {
        emit recognitionReady({}, "請選擇視覺模型");
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 20 * 1024 * 1024)
    {
        emit recognitionReady({}, "圖片無法讀取、為空或超過 20 MB");
        return;
    }
    QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 40000000)
    {
        emit recognitionReady({}, "圖片格式無效或超過 4,000 萬像素");
        return;
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        emit recognitionReady({}, "讀取圖片失敗");
        return;
    }
    QJsonObject body{{"model", model.trimmed()},
                     {"stream", false},
                     {"format", "json"},
                     {"prompt", model.trimmed().startsWith("glm-ocr", Qt::CaseInsensitive)
                                    ? QString("Text Recognition:")
                                    : WaveformOcr::recognitionPrompt()},
                     {"images", QJsonArray{QString::fromLatin1(bytes.toBase64())}},
                     {"options", QJsonObject{{"temperature", 0}, {"num_predict", 4096}}}};
    request(base, QJsonDocument(body).toJson(QJsonDocument::Compact), false);
}

void OllamaClient::request(const QUrl &base, const QByteArray &body, bool models)
{
    if (busy())
        return;
    const auto error = validateEndpoint(base);
    if (!error.isEmpty())
    {
        if (models)
            emit modelsReady({}, error);
        else
            emit recognitionReady({}, error);
        return;
    }
    QUrl url(base);
    url.setPath(models ? "/api/tags" : "/api/generate");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply = models ? m_network.get(request) : m_network.post(request, body);
    m_reply = reply;
    m_timer.start(models ? qMin(m_timeoutMs, 10000) : m_timeoutMs);
    // Bound accumulated reply data, including a server that never closes the connection.
    auto buffer = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this,
            [reply, buffer]
            {
                buffer->append(reply->readAll());
                if (buffer->size() > 1024 * 1024)
                {
                    reply->setProperty("ocrTooLarge", true);
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, buffer, models]
            {
                if (m_reply != reply)
                {
                    reply->deleteLater();
                    return;
                }
                m_timer.stop();
                m_reply.clear();
                buffer->append(reply->readAll());
                const auto doc = QJsonDocument::fromJson(*buffer);
                const auto object = doc.object();
                QString error;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (reply->property("ocrTimeout").toBool())
                    error = "Ollama 回應逾時";
                else if (reply->property("ocrTooLarge").toBool() || buffer->size() > 1024 * 1024)
                    error = "Ollama 回應超過 1 MB";
                else if (!object.value("error").toString().isEmpty())
                    error = object.value("error").toString();
                else if (reply->error() != QNetworkReply::NoError)
                    error = reply->errorString();
                else if (status != 200)
                    error = QString("Ollama HTTP %1").arg(status);
                else if (!doc.isObject())
                    error = "Ollama 回應不是有效 JSON";
                QStringList names;
                QString text;
                if (error.isEmpty() && models)
                {
                    if (!object.value("models").isArray())
                        error = "Ollama 缺少模型清單";
                    for (const auto &entry : object.value("models").toArray())
                    {
                        const auto name = entry.toObject().value("name").toString();
                        if (!name.isEmpty())
                            names << name;
                    }
                }
                else if (error.isEmpty())
                {
                    text = object.value("response").toString();
                    if (!object.value("done").toBool() || object.value("done_reason").toString() == "length")
                        error = "Ollama 回應未完整結束";
                    else if (text.trimmed().isEmpty())
                        error = "Ollama 沒有回傳辨識文字";
                }
                reply->deleteLater();
                if (models)
                    emit modelsReady(names, error);
                else
                    emit recognitionReady(text, error);
            });
}

void OllamaClient::cancel()
{
    m_timer.stop();
    if (!m_reply)
        return;
    auto *reply = m_reply.data();
    m_reply.clear(); // abort() can synchronously emit finished; it must not deliver an old result.
    reply->abort();
    reply->deleteLater();
}
