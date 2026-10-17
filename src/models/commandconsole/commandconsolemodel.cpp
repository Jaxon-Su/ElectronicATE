#include "commandconsolemodel.h"
#include <QDateTime>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDebug>
#include <QSet>

CommandConsoleModel::CommandConsoleModel(QObject* parent) : QObject(parent)
{
    // 預設地址歷史（示例）
    m_addressHistory << "TCPIP0::192.168.0.3::INSTR";
}

// 連線狀態

void CommandConsoleModel::setConnectionStatus(ConnectionStatus status)
{
    if (m_connectionStatus != status) {
        m_connectionStatus = status;
        emit connectionStatusChanged(status);
    }
}

void CommandConsoleModel::setCurrentAddress(const QString& address)
{
    if (m_currentAddress != address) {
        m_currentAddress = address;
        emit currentAddressChanged(address);
    }
}

// 配置

void CommandConsoleModel::setTimeout(int ms)
{
    int bounded = qBound(MIN_TIMEOUT, ms, MAX_TIMEOUT);
    if (m_timeout != bounded) {
        m_timeout = bounded;
        emit timeoutChanged(bounded);
    }
}

// 地址歷史

void CommandConsoleModel::setAddressHistory(const QStringList& history)
{
    m_addressHistory = history;

    // 限制數量
    while (m_addressHistory.size() > MAX_ADDRESS_HISTORY) {
        m_addressHistory.removeLast();
    }

    emit addressHistoryChanged();
}

void CommandConsoleModel::addToAddressHistory(const QString& address)
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

void CommandConsoleModel::clearAddressHistory()
{
    if (!m_addressHistory.isEmpty()) {
        m_addressHistory.clear();
        emit addressHistoryChanged();
    }
}

// 命令歷史

void CommandConsoleModel::setCommandHistory(const QStringList& history)
{
    m_commandHistory = history;

    // 限制數量
    while (m_commandHistory.size() > MAX_COMMAND_HISTORY) {
        m_commandHistory.removeLast();
    }

    // qDebug() << "[CommandConsoleModel] setCommandHistory called, count:" << m_commandHistory.size();

    emit commandHistoryChanged();
}

void CommandConsoleModel::addToCommandHistory(const QString& command)
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

void CommandConsoleModel::clearCommandHistory()
{
    if (!m_commandHistory.isEmpty()) {
        m_commandHistory.clear();
        emit commandHistoryChanged();
    }
}

// 完整通訊記錄

void CommandConsoleModel::addToFullHistory(const CommandHistory& entry)
{
    m_fullHistory.prepend(entry);

    while (m_fullHistory.size() > MAX_FULL_HISTORY) {
        m_fullHistory.removeLast();
    }

    emit fullHistoryChanged();
}

void CommandConsoleModel::clearFullHistory()
{
    if (!m_fullHistory.isEmpty()) {
        m_fullHistory.clear();
        emit fullHistoryChanged();
    }
}

void CommandConsoleModel::recordCommand(const QString& command, const QString& response, bool success)
{
    CommandHistory entry;
    entry.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    entry.address = m_currentAddress;
    entry.command = command;
    entry.response = response;
    entry.success = success;

    addToFullHistory(entry);
}

// XML 序列化

void CommandConsoleModel::writeXml(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("CommandConsole");

    // 儲存 Timeout 設定
    writer.writeTextElement("Timeout", QString::number(m_timeout));

    // 儲存地址歷史
    writer.writeStartElement("AddressHistory");
    for (const QString& addr : m_addressHistory) {
        writer.writeTextElement("Address", addr);
    }
    writer.writeEndElement(); // AddressHistory

    // 儲存命令歷史
    writer.writeStartElement("CommandHistory");
    for (const QString& cmd : m_commandHistory) {
        writer.writeTextElement("Command", cmd);
    }
    writer.writeEndElement(); // CommandHistory

    // 儲存完整通訊記錄
    writer.writeStartElement("FullHistory");
    for (const CommandHistory& entry : m_fullHistory) {
        writer.writeStartElement("Entry");
        writer.writeAttribute("success", entry.success ? "1" : "0");
        writer.writeTextElement("Timestamp", entry.timestamp);
        writer.writeTextElement("Address", entry.address);
        writer.writeTextElement("Command", entry.command);
        writer.writeTextElement("Response", entry.response);
        writer.writeEndElement(); // Entry
    }
    writer.writeEndElement(); // FullHistory

    writer.writeEndElement(); // CommandConsole
}

void CommandConsoleModel::loadXml(QXmlStreamReader& reader)
{
    CommandConsoleModel candidate;
    candidate.readXmlFields(reader);
    if (reader.hasError())
        return;
    setTimeout(candidate.m_timeout);
    setAddressHistory(candidate.m_addressHistory);
    setCommandHistory(candidate.m_commandHistory);
    m_fullHistory = std::move(candidate.m_fullHistory);
    emit fullHistoryChanged();
}

void CommandConsoleModel::readXmlFields(QXmlStreamReader& reader)
{
    if (!reader.isStartElement() || reader.name() != "CommandConsole") {
        reader.raiseError("Expected CommandConsole element");
        return;
    }

    QSet<QString> fields;
    while (reader.readNextStartElement()) {
        const QString field = reader.name().toString();
        if (fields.contains(field)) {
            reader.raiseError("Duplicate CommandConsole field: " + field);
            return;
        }
        fields.insert(field);
        if (field == "Timeout") {
            bool ok = false;
            const int timeout = reader.readElementText().toInt(&ok);
            if (!ok || timeout < MIN_TIMEOUT || timeout > MAX_TIMEOUT) {
                reader.raiseError("Invalid CommandConsole timeout");
                return;
            }
            m_timeout = timeout;
        } else if (field == "AddressHistory" || field == "CommandHistory") {
            const QString item = field == "AddressHistory" ? "Address" : "Command";
            QStringList values;
            while (reader.readNextStartElement()) {
                if (reader.name() != item) {
                    reader.raiseError("Unexpected CommandConsole history item: " + reader.name().toString());
                    return;
                }
                const QString value = reader.readElementText().trimmed();
                if (!value.isEmpty())
                    values.append(value);
            }
            if (field == "AddressHistory")
                setAddressHistory(values);
            else
                setCommandHistory(values);
        } else if (field == "FullHistory") {
            while (reader.readNextStartElement()) {
                const auto success = reader.attributes().value("success");
                if (reader.name() != "Entry" || (success != "0" && success != "1")) {
                    reader.raiseError("Expected CommandConsole Entry with success 0 or 1");
                    return;
                }
                CommandHistory entry;
                entry.success = success == "1";
                QSet<QString> entryFields;
                while (reader.readNextStartElement()) {
                    const QString key = reader.name().toString();
                    if (entryFields.contains(key)) {
                        reader.raiseError("Duplicate CommandConsole Entry field: " + key);
                        return;
                    }
                    entryFields.insert(key);
                    const QString value = reader.readElementText();
                    if (key == "Timestamp") entry.timestamp = value;
                    else if (key == "Address") entry.address = value;
                    else if (key == "Command") entry.command = value;
                    else if (key == "Response") entry.response = value;
                    else {
                        reader.raiseError("Unexpected CommandConsole Entry field: " + key);
                        return;
                    }
                }
                if (entryFields != QSet<QString>{"Timestamp", "Address", "Command", "Response"}) {
                    reader.raiseError("Incomplete CommandConsole Entry");
                    return;
                }
                if (m_fullHistory.size() < MAX_FULL_HISTORY)
                    m_fullHistory.append(entry);
            }
        } else {
            reader.raiseError("Unexpected CommandConsole field: " + field);
            return;
        }
    }
    if (!reader.hasError() && fields != QSet<QString>{"Timeout", "AddressHistory", "CommandHistory", "FullHistory"})
        reader.raiseError("Incomplete CommandConsole configuration");
}
