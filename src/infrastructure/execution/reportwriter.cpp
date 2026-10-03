#include "reportwriter.h"
#include "page5excelreport.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <exception>

ReportWriter::ReportWriter(QObject *parent)
    : ReportWriter(
          [](const QString &directory, const QString &name, const QVector<Page5ResultRecord> &records) {
              ReportWriteResult result;
              result.success = Page5ExcelReport::save(directory, name, records, result.path, result.error);
              return result;
          },
          parent)
{
}
ReportWriter::ReportWriter(Save save, QObject *parent) : QObject(parent), m_save(std::move(save)) {}
bool ReportWriter::submit(QString directory, QString name, QVector<Page5ResultRecord> records)
{
    if (m_busy || !m_save)
        return false;
    m_busy = true;
    auto *watcher = new QFutureWatcher<ReportWriteResult>(this);
    connect(watcher, &QFutureWatcher<ReportWriteResult>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        emit completed(result.success, result.path, result.error);
    });
    // Work owns its snapshot and callable; no ViewModel/QObject is used off-thread.
    watcher->setFuture(QtConcurrent::run([save = m_save, directory = std::move(directory),
                                          name = std::move(name), records = std::move(records)] {
        try {
            return save(directory, name, records);
        } catch (const std::exception &error) {
            return ReportWriteResult{false, {}, QString::fromUtf8(error.what())};
        } catch (...) {
            return ReportWriteResult{false, {}, "Unexpected report write error"};
        }
    }));
    return true;
}
