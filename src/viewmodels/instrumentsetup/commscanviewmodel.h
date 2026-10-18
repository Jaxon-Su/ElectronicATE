#pragma once
#include "../../data/commscanentry.h"
#include <QList>
#include <QObject>
#include <QStringList>

class CommScanProcess;

class CommScanViewModel : public QObject {
    Q_OBJECT
  public:
    explicit CommScanViewModel(CommScanProcess *process, QObject *parent = nullptr);
    const QList<CommScanEntry> &entries() const { return m_entries; }
    bool isRunning() const { return m_running; }
    void start();
    void stop();
  signals:
    void entriesChanged();
    void runningChanged(bool running);
    void statusChanged(const QString &status);
  private:
    void merge(CommScanEntry entry);
    CommScanProcess *m_process;
    QList<CommScanEntry> m_entries;
    QStringList m_warnings;
    bool m_running = false;
    bool m_stopping = false;
};
