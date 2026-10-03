#include "commscanviewmodel.h"
#include "../../infrastructure/discovery/commscanprocess.h"

CommScanViewModel::CommScanViewModel(CommScanProcess *process, QObject *parent)
    : QObject(parent), m_process(process)
{
    connect(process, &CommScanProcess::messageReceived, this, [this](const QJsonObject &message) {
        if (!m_running || m_stopping)
            return;
        if (message.contains("entry"))
            merge(CommScanEntry::fromJson(message.value("entry").toObject()));
        if (message.contains("warning"))
            m_warnings << message.value("warning").toString();
    });
    connect(process, &CommScanProcess::finished, this, [this](const QString &error) {
        if (!m_running)
            return;
        if (!error.isEmpty() && !m_stopping)
            m_warnings << error;
        m_running = false;
        emit runningChanged(false);
        emit statusChanged(m_stopping ? "Stopped. Partial results retained."
            : QString("Scan finished: %1 resources. %2").arg(m_entries.size()).arg(m_warnings.join(" ")).trimmed());
    });
}

void CommScanViewModel::start()
{
    if (m_running)
        return;
    m_entries.clear();
    m_warnings.clear();
    m_stopping = false;
    m_running = true;
    emit entriesChanged();
    emit runningChanged(true);
    emit statusChanged("Listing VISA resources and COM ports ...");
    m_process->start();
}

void CommScanViewModel::stop()
{
    if (!m_running)
        return;
    m_stopping = true;
    m_process->stop();
}

void CommScanViewModel::merge(CommScanEntry entry)
{
    if (entry.address.isEmpty())
        return;
    for (auto &current : m_entries) {
        if (current.key() == entry.key()) {
            if (!entry.description.isEmpty())
                current.description = entry.description;
            if (!entry.visaAddress.isEmpty())
                current.visaAddress = entry.visaAddress;
            auto sources = (current.source + " / " + entry.source).split(" / ", Qt::SkipEmptyParts);
            sources.removeDuplicates();
            sources.sort();
            current.source = sources.join(" / ");
            emit entriesChanged();
            return;
        }
    }
    m_entries.append(std::move(entry));
    emit entriesChanged();
}
