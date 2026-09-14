#pragma once

#include <QObject>
#include <memory>
#include <functional>
#include <atomic>
class ConsoleSession;
class QThread;
#include "page4config.h"
#include "ixmlserializable.h"

class Page4Model;
class ICommunication;
class QXmlStreamWriter;
class QXmlStreamReader;

class Page4ViewModel : public QObject, public IXmlSerializable
{
    Q_OBJECT

public:
    using CommunicationCreator = std::function<std::unique_ptr<ICommunication>(const QString&)>;
    explicit Page4ViewModel(Page4Model *model, QObject *parent = nullptr);
    Page4ViewModel(Page4Model* model, CommunicationCreator create, QObject* parent = nullptr);
    ~Page4ViewModel() override;

    // ==================== 連線操作 ====================
    void connectToAddress(const QString &address);
    void disconnect();
    bool isConnected() const;
    bool isControlActive() const;
    bool isOperationPending() const { return m_connectionPending || m_commandInProgress; }
    void setControlAllowed(bool allowed) { m_controlAllowed = allowed; }

    // ==================== 指令操作 ====================
    void sendCommand(const QString &command);
    void sendQuery(const QString &command);

    // ==================== 常用指令 ====================
    void sendIDN();      // *IDN?
    void sendRST();      // *RST
    void sendCLS();      // *CLS
    void sendOPC();      // *OPC?

    // ==================== 歷史管理 ====================
    void clearAllHistory();

    // ==================== 配置 ====================
    void setTimeout(int ms);
    int timeout() const;

    // ==================== XML 序列化 ====================
    QString xmlTagName() const override { return "Page4"; }
    void writeXml(QXmlStreamWriter &writer) const override;
    void publishXmlLoaded() override;
    void validateXml(QXmlStreamReader& reader) const override;
    void loadXml(QXmlStreamReader &reader) override;

    // ==================== Model 存取 ====================
    Page4Model* model() const { return m_model; }

signals:
    void controlActiveChanged(bool active);
    // 連線狀態變更（帶描述訊息）
    void connectionStatusChanged(ConnectionStatus status, const QString &message);

    // 收到回應
    void responseReceived(const QString &command, const QString &response);

    // 錯誤發生
    void errorOccurred(const QString &error);

    // 命令已發送（用於 UI 日誌）
    void commandSent(const QString &command);

    // 歷史更新（通知 UI 刷新下拉選單）
    void historyUpdated();

private:
    Page4Model *m_model = nullptr;
    ConsoleSession* m_session = nullptr;
    QThread* m_thread = nullptr;
    std::shared_ptr<std::atomic<quint64>> m_revision;
    bool m_connected = false;
    bool m_connectionPending = false;
    quint64 m_commandRevision = 0;
    quint64 m_connectionRevision = 0;
    bool m_commandInProgress = false;
    bool m_controlAllowed = true;
    bool m_connectionActive = false;

};
