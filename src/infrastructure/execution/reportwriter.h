#pragma once
#include "page5resultrecord.h"
#include <QObject>
#include <functional>

struct ReportWriteResult {
    bool success = false;
    QString path;
    QString error;
};
class ReportWriter : public QObject
{
    Q_OBJECT
  public:
    using Save = std::function<ReportWriteResult(const QString &, const QString &,
                                                 const QVector<Page5ResultRecord> &)>;
    explicit ReportWriter(QObject *parent = nullptr);
    ReportWriter(Save save, QObject *parent);
    bool isBusy() const { return m_busy; }
    bool submit(QString directory, QString name, QVector<Page5ResultRecord> records);
  signals:
    void completed(bool success, const QString &path, const QString &error);

  private:
    Save m_save;
    bool m_busy = false;
};
