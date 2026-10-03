#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

// One in-flight request; all callbacks run on the owning event loop.
class OllamaClient : public QObject
{
    Q_OBJECT
  public:
    explicit OllamaClient(QObject *parent = nullptr);
    ~OllamaClient() override;
    static QString validateEndpoint(const QUrl &url);
    void listModels(const QUrl &base);
    void recognize(const QUrl &base, const QString &model, const QString &imagePath);
    void cancel();
    bool busy() const { return !m_reply.isNull(); }
    void setTimeoutMs(int milliseconds) { m_timeoutMs = qMax(1, milliseconds); }
  signals:
    void modelsReady(const QStringList &models, const QString &error);
    void recognitionReady(const QString &text, const QString &error);

  private:
    void request(const QUrl &base, const QByteArray &body, bool models);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timer;
    int m_timeoutMs = 900000;
};
