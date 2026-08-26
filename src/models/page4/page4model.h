#pragma once

#include <QObject>
#include <QStringList>
#include "page4config.h"

class QXmlStreamWriter;
class QXmlStreamReader;

class Page4Model : public QObject
{
    Q_OBJECT

public:
    explicit Page4Model(QObject *parent = nullptr);
    ~Page4Model() override = default;

    // ==================== 連線狀態 ====================
    ConnectionStatus connectionStatus() const { return m_connectionStatus; }
    void setConnectionStatus(ConnectionStatus status);

    QString currentAddress() const { return m_currentAddress; }
    void setCurrentAddress(const QString &address);

    // ==================== 配置 ====================
    int timeout() const { return m_timeout; }
    void setTimeout(int ms);

    // ==================== 地址歷史 ====================
    QStringList addressHistory() const { return m_addressHistory; }
    void setAddressHistory(const QStringList &history);
    void addToAddressHistory(const QString &address);
    void clearAddressHistory();

    // ==================== 命令歷史 ====================
    QStringList commandHistory() const { return m_commandHistory; }
    void setCommandHistory(const QStringList &history);
    void addToCommandHistory(const QString &command);
    void clearCommandHistory();

    // ==================== 完整通訊記錄 ====================
    QList<CommandHistory> fullHistory() const { return m_fullHistory; }
    void addToFullHistory(const CommandHistory &entry);
    void clearFullHistory();

    // 便捷方法：記錄一筆通訊
    void recordCommand(const QString &command, const QString &response, bool success);

    // ==================== XML 序列化 ====================
    void writeXml(QXmlStreamWriter &writer) const;
    void loadXml(QXmlStreamReader &reader);

    // ==================== 配置常數 ====================
    static constexpr int MAX_ADDRESS_HISTORY = 20;
    static constexpr int MAX_COMMAND_HISTORY = 50;
    static constexpr int MAX_FULL_HISTORY = 200;
    static constexpr int DEFAULT_TIMEOUT = 5000;
    static constexpr int MIN_TIMEOUT = 100;
    static constexpr int MAX_TIMEOUT = 60000;

signals:
    void connectionStatusChanged(ConnectionStatus status);
    void currentAddressChanged(const QString &address);
    void timeoutChanged(int ms);
    void addressHistoryChanged();
    void commandHistoryChanged();
    void fullHistoryChanged();

private:
    // 連線狀態
    ConnectionStatus m_connectionStatus = ConnectionStatus::Disconnected;
    QString m_currentAddress;

    // 配置
    int m_timeout = DEFAULT_TIMEOUT;

    // 歷史記錄
    QStringList m_addressHistory;
    QStringList m_commandHistory;
    QList<CommandHistory> m_fullHistory;
};
