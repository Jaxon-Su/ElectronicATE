#pragma once
#include "capturefile.h"
#include "../../hardware/communication/transfercancellation.h"
#include "captureoptions.h"
#include "page5resultrecord.h"
#include "scopecapturerecord.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QScopeGuard>
#include <QRegularExpression>
#include <QSet>
#include <limits>
#include <stdexcept>

// One collector per task attempt; every completed acquisition is retained.
class StrategyWaveformCapture
{
  public:
    struct Attempt {
        int taskNumber; // 1-based Page5 row
        int attemptNumber; // 1 = first execution, 2 = first retry
        QDateTime startedAt;
    };
    StrategyWaveformCapture(CaptureOptions settings, QString directory,
                            QAtomicInt &stop, QString ac, QString dc, QString strategy, const Attempt &attempt)
        : m_settings(std::move(settings)), m_stop(stop),
          m_ac(std::move(ac)), m_dc(std::move(dc)), m_strategy(std::move(strategy))
    {
        m_directory = m_settings.directory.isEmpty() ? directory : m_settings.directory;
        if (m_settings.formats.isEmpty() || !QDir::isAbsolutePath(m_directory) ||
            !QFileInfo(m_directory).isDir() || !QFileInfo(m_directory).isWritable())
            fail("Capture: select formats and an existing writable directory in Capture settings or Report File Path");
        if (attempt.taskNumber < 1 || attempt.attemptNumber < 1 || !attempt.startedAt.isValid())
            fail("Capture: invalid task attempt identity");
        m_folderName = QString("%1_Task%2_%3_Attempt%4")
            .arg(attempt.startedAt.toLocalTime().toString("yyyyMMdd_HHmmss"))
            .arg(attempt.taskNumber, 2, 10, QChar('0'))
            .arg(component(m_strategy))
            .arg(attempt.attemptNumber, 2, 10, QChar('0'));
    }

    void record(IScopeCapture *scope, const ScopeCaptureRecord &record,
                const Page5ConditionSource &condition)
    {
        const TransferCancellation cancellation(m_stop);
        check();
        requireStopped(scope);
        const bool single = m_settings.formats.contains("WFM") || m_settings.formats.contains("CSV");
        const bool allChannels = m_settings.formats.contains("AllWFM") || m_settings.formats.contains("AllCSV");
        int selected = m_settings.channel;
        if (single && selected == 0) {
            selected = scope->captureSource().channel;
            checkInstrument(scope);
        }
        if (single || allChannels) {
            QSet<int> channels;
            for (const auto &value : record.channels) {
                if (value.channel < 1 || value.channel > scope->getTotalChannel())
                    fail("Capture: invalid waveform channel");
                channels.insert(value.channel);
            }
            if (channels.isEmpty() || (single && !channels.contains(selected)))
                fail("Capture: selected channel is unavailable");
        }
        createAttemptDirectory();
        const QString load = condition.index < 0 ? "Load_None"
            : QString("%1%2_%3").arg(condition.kind == LoadConditionKind::Dynamic ? "DyLoad" : "Load")
                  .arg(condition.index + 1).arg(condition.label.isEmpty() ? "Unnamed" : condition.label);
        const QString id = QString("%1_%2_%3_%4_%5")
            .arg(component(m_ac), component(m_dc), component(load), component(m_strategy))
            .arg(++m_sequence, 4, 10, QChar('0'));
        const QString stem = QDir(m_directory).filePath(id);
        QStringList saved;
        auto rollback = qScopeGuard([&] { for (const auto &path : saved) QFile::remove(path); });
        if (m_settings.formats.contains("PNG")) {
            check();
            if (!scope->waitForOperationComplete(15000)) fail("Capture: instrument not ready to save screenshot");
            saved << stem + ".png";
            if (!CaptureFile::screenshot(scope, saved.last(), "PNG")) fail("Capture: PNG save failed");
        }
        QSet<QString> transfers;
        for (const auto &format : m_settings.formats) {
            if (format == "PNG") continue;
            const bool all = format.startsWith("All");
            const QString kind = all ? format.mid(3) : format;
            for (const auto &value : record.channels) {
                if (!all && value.channel != selected) continue;
                const QString suffix = QString("_CH%1.%2").arg(value.channel).arg(kind.toLower());
                if (transfers.contains(suffix)) continue;
                check();
                saved << stem + suffix;
                if (!CaptureFile::waveform(scope, value.channel, saved.last(), kind))
                    fail("Capture: " + suffix + " save failed: " + scope->lastError());
                transfers.insert(suffix);
            }
        }
        check();
        requireStopped(scope);
        check();
        rollback.dismiss();
        m_files += saved;
        ++m_savedRecords;
    }
    QStringList files() const { return m_files; }
    QString summary() const {
        return m_savedRecords ? QString("Capture: %1 completed records; %2").arg(m_savedRecords).arg(m_directory) : QString{};
    }
  private:
    void createAttemptDirectory() {
        if (m_created) return;
        const QDir parent(m_directory);
        for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
            check();
            const QString folder = suffix == 1 ? m_folderName
                : m_folderName + QString("_%1").arg(suffix, 2, 10, QChar('0'));
            // mkdir claims a new directory; an existing entry is never reused.
            if (parent.mkdir(folder)) {
                m_directory = parent.filePath(folder);
                m_created = true;
                return;
            }
            const QFileInfo existing(parent.filePath(folder));
            if (!existing.exists() && !existing.isSymLink())
                fail("Capture: cannot create attempt directory: " + existing.filePath());
        }
        fail("Capture: no available attempt directory name");
    }
    static QString component(QString text) {
        text.replace(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f\\s]+"), "_");
        text = text.left(32);
        while (text.endsWith('.') || text.endsWith('_')) text.chop(1);
        return text.isEmpty() ? "Unnamed" : text;
    }
    [[noreturn]] static void fail(const QString &error) { throw std::runtime_error(error.toStdString()); }
    static void checkInstrument(IScopeCapture *scope) {
        if (!scope->lastError().isEmpty()) fail("Capture readback: " + scope->lastError());
    }
    void check() const { if (m_stop.loadAcquire()) fail("Capture: stopped by user"); }
    static void requireStopped(IScopeCapture *scope) {
        if (!scope || scope->acquisitionState() != ScopeAcquisitionState::Completed)
            fail("Capture: acquisition is not stopped");
    }
    CaptureOptions m_settings;
    QString m_directory, m_folderName;
    QAtomicInt &m_stop;
    QString m_ac, m_dc, m_strategy;
    QStringList m_files;
    bool m_created = false;
    int m_sequence = 0, m_savedRecords = 0;
};
