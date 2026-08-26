#include "page4model.h"
#include <QDateTime>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDebug>

Page4Model::Page4Model(QObject *parent)
    : QObject(parent)
{
    // 預設地址歷史（示例）
    m_addressHistory << "TCPIP0::192.168.0.3::INSTR";
}

// ==================== 連線狀態 ====================

void Page4Model::setConnectionStatus(ConnectionStatus status)
{
    if (m_connectionStatus != status) {
        m_connectionStatus = status;
        emit connectionStatusChanged(status);
    }
}

void Page4Model::setCurrentAddress(const QString &address)
{
    if (m_currentAddress != address) {
        m_currentAddress = address;
        emit currentAddressChanged(address);
    }
}

// ==================== 配置 ====================

void Page4Model::setTimeout(int ms)
{
    int bounded = qBound(MIN_TIMEOUT, ms, MAX_TIMEOUT);
    if (m_timeout != bounded) {
        m_timeout = bounded;
        emit timeoutChanged(bounded);
    }
}

// ==================== 地址歷史 ====================

void Page4Model::setAddressHistory(const QStringList &history)
{
    m_addressHistory = history;

    // 限制數量
    while (m_addressHistory.size() > MAX_ADDRESS_HISTORY) {
        m_addressHistory.removeLast();
    }

    emit addressHistoryChanged();
}

void Page4Model::addToAddressHistory(const QString &address)
{
    QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    // 移除已存在的相同地址（避免重複）
    m_addressHistory.removeAll(trimmed);

    // 插入到最前面
    m_addressHistory.prepend(trimmed);

    // 限制數量
    while (m_addressHistory.size() > MAX_ADDRESS_HISTORY) {
        m_addressHistory.removeLast();
    }

    emit addressHistoryChanged();
}

void Page4Model::clearAddressHistory()
{
    if (!m_addressHistory.isEmpty()) {
        m_addressHistory.clear();
        emit addressHistoryChanged();
    }
}

// ==================== 命令歷史 ====================

void Page4Model::setCommandHistory(const QStringList &history)
{
    m_commandHistory = history;

    // 限制數量
    while (m_commandHistory.size() > MAX_COMMAND_HISTORY) {
        m_commandHistory.removeLast();
    }

    // qDebug() << "[Page4Model] setCommandHistory called, count:" << m_commandHistory.size();

    emit commandHistoryChanged();
}

void Page4Model::addToCommandHistory(const QString &command)
{
    QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_commandHistory.removeAll(trimmed);
    m_commandHistory.prepend(trimmed);

    while (m_commandHistory.size() > MAX_COMMAND_HISTORY) {
        m_commandHistory.removeLast();
    }

    emit commandHistoryChanged();
}

void Page4Model::clearCommandHistory()
{
    if (!m_commandHistory.isEmpty()) {
        m_commandHistory.clear();
        emit commandHistoryChanged();
    }
}

// ==================== 完整通訊記錄 ====================

void Page4Model::addToFullHistory(const CommandHistory &entry)
{
    m_fullHistory.prepend(entry);

    while (m_fullHistory.size() > MAX_FULL_HISTORY) {
        m_fullHistory.removeLast();
    }

    emit fullHistoryChanged();
}

void Page4Model::clearFullHistory()
{
    if (!m_fullHistory.isEmpty()) {
        m_fullHistory.clear();
        emit fullHistoryChanged();
    }
}

void Page4Model::recordCommand(const QString &command,
                               const QString &response,
                               bool success)
{
    CommandHistory entry;
    entry.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    entry.address = m_currentAddress;
    entry.command = command;
    entry.response = response;
    entry.success = success;

    addToFullHistory(entry);
}

// ==================== XML 序列化 ====================

void Page4Model::writeXml(QXmlStreamWriter &writer) const
{
    writer.writeStartElement("Page4");

    // 儲存 Timeout 設定
    writer.writeTextElement("Timeout", QString::number(m_timeout));

    // 儲存地址歷史
    writer.writeStartElement("AddressHistory");
    for (const QString &addr : m_addressHistory) {
        writer.writeTextElement("Address", addr);
    }
    writer.writeEndElement(); // AddressHistory

    // 儲存命令歷史
    writer.writeStartElement("CommandHistory");
    for (const QString &cmd : m_commandHistory) {
        writer.writeTextElement("Command", cmd);
    }
    writer.writeEndElement(); // CommandHistory

    // 儲存完整通訊記錄
    writer.writeStartElement("FullHistory");
    for (const CommandHistory &entry : m_fullHistory) {
        writer.writeStartElement("Entry");
        writer.writeAttribute("success", entry.success ? "1" : "0");
        writer.writeTextElement("Timestamp", entry.timestamp);
        writer.writeTextElement("Address",   entry.address);
        writer.writeTextElement("Command",   entry.command);
        writer.writeTextElement("Response",  entry.response);
        writer.writeEndElement(); // Entry
    }
    writer.writeEndElement(); // FullHistory

    writer.writeEndElement(); // Page4
}

void Page4Model::loadXml(QXmlStreamReader &reader)
{
    // qDebug() << "[Page4Model::loadXml] Started, current element:" << reader.name();

    // 確保我們在 Page4 元素內
    if (reader.name().toString() != "Page4") {
        // qDebug() << "[Page4Model::loadXml] Not at Page4 element, returning";
        return;
    }

    QStringList addresses;
    QStringList commands;
    bool inAddressHistory = false;
    bool inCommandHistory = false;
    bool inFullHistory    = false;
    CommandHistory currentEntry;

    m_fullHistory.clear();

    while (!reader.atEnd() && !reader.hasError()) {
        QXmlStreamReader::TokenType token = reader.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString elementName = reader.name().toString();

            if (elementName == "Timeout") {
                bool ok;
                int timeout = reader.readElementText().toInt(&ok);
                if (ok) setTimeout(timeout);
            }
            else if (elementName == "AddressHistory") {
                inAddressHistory = true;
                addresses.clear();
            }
            else if (elementName == "Address" && inAddressHistory) {
                QString addr = reader.readElementText().trimmed();
                if (!addr.isEmpty()) addresses.append(addr);
            }
            else if (elementName == "CommandHistory") {
                inCommandHistory = true;
                commands.clear();
            }
            else if (elementName == "Command" && inCommandHistory) {
                QString cmd = reader.readElementText().trimmed();
                if (!cmd.isEmpty()) commands.append(cmd);
            }
            else if (elementName == "FullHistory") {
                inFullHistory = true;
            }
            else if (elementName == "Entry" && inFullHistory) {
                currentEntry = CommandHistory{};
                currentEntry.success = (reader.attributes().value("success").toString() == "1");
            }
            else if (inFullHistory) {
                if      (elementName == "Timestamp") currentEntry.timestamp = reader.readElementText();
                else if (elementName == "Address")   currentEntry.address   = reader.readElementText();
                else if (elementName == "Command")   currentEntry.command   = reader.readElementText();
                else if (elementName == "Response")  currentEntry.response  = reader.readElementText();
            }
        }
        else if (token == QXmlStreamReader::EndElement) {
            QString elementName = reader.name().toString();

            if (elementName == "AddressHistory") {
                inAddressHistory = false;
                setAddressHistory(addresses);
            }
            else if (elementName == "CommandHistory") {
                inCommandHistory = false;
                setCommandHistory(commands);
            }
            else if (elementName == "Entry" && inFullHistory) {
                if (m_fullHistory.size() < MAX_FULL_HISTORY)
                    m_fullHistory.append(currentEntry);
            }
            else if (elementName == "FullHistory") {
                inFullHistory = false;
                emit fullHistoryChanged();
            }
            else if (elementName == "Page4") {
                break;
            }
        }
    }
}
