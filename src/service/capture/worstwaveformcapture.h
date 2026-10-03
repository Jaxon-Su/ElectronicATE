#pragma once
#include "capturefile.h"
#include "../../hardware/communication/transfercancellation.h"
#include "../../data/captureoptions.h"
#include "../../data/page5resultrecord.h"
#include "../oscilloscopestrategy/ioscilloscopemeasurestrategy.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QScopeGuard>
#include <QUuid>
#include <QVariantMap>
#include <QMap>
#include <QSet>
#include <limits>
#include <stdexcept>

// One instance per task attempt. Winners change only after every requested file is saved.
class WorstWaveformCapture
{
  public:
    WorstWaveformCapture(QVariantMap settings, QString target, QString directory, QString task,
                         QAtomicInt &stop)
        : WorstWaveformCapture(CaptureOptions(settings, true), std::move(target), std::move(directory),
                               std::move(task), stop)
    {
    }
    WorstWaveformCapture(CaptureOptions settings, QString target, QString directory, QString task,
                         QAtomicInt &stop)
        : m_settings(std::move(settings)), m_target(std::move(target)), m_task(std::move(task)), m_stop(stop)
    {
        m_formats = m_settings.formats;
        if (m_formats.isEmpty())
            fail("Capture: select at least one format");
        for (const auto &format : m_formats)
            if (!QStringList{"WFM", "AllWFM", "CSV", "AllCSV", "PNG"}.contains(format))
                fail("Capture: invalid format " + format);
        const QString configured = m_settings.directory;
        m_directory = configured.isEmpty() ? directory : configured;
        if (!QDir::isAbsolutePath(m_directory) || !QFileInfo(m_directory).isDir() ||
            !QFileInfo(m_directory).isWritable())
            fail("Capture: select an existing writable directory in Capture settings or Report File Path");
        m_prefix = "Capture_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    void record(IScopeCapture *scope, const QVector<OscChannelMeasure> &values,
                const Page5ConditionSource &condition = {})
    {
        const TransferCancellation transferCancellation(m_stop);
        check();
        ++m_sequence;
        const auto triggerSource = scope->captureSource();
        const QString source = triggerSource.name;
        const int trigger = triggerSource.channel;
        if (trigger < 1 || trigger > scope->getTotalChannel())
            fail("Capture: invalid trigger source");
        const bool single = m_formats.contains("WFM") || m_formats.contains("CSV");
        const int selected = !single || m_settings.channel == 0 ? trigger : m_settings.channel;
        const bool all = m_formats.contains("AllWFM") || m_formats.contains("AllCSV");
        if ((single || !all) &&
            (selected < 1 || selected > scope->getTotalChannel() || !scope->isChannelEnabled(selected)))
            fail("Capture: selected channel is unavailable");
        QMap<QString, double> improvements;
        for (const auto &value : values) {
            if (!all && value.channel != selected)
                continue;
            const QString ch = QString("CH%1 ").arg(value.channel);
            if (m_target != "MIN" &&
                (!m_winners.contains(ch + "Max") || value.maxVal > m_winners[ch + "Max"].value))
                improvements[ch + "Max"] = value.maxVal;
            if (m_target != "MAX" &&
                (!m_winners.contains(ch + "Min") || value.minVal < m_winners[ch + "Min"].value))
                improvements[ch + "Min"] = value.minVal;
        }
        if (improvements.isEmpty())
            return;
        requireStopped(scope);
        const double level = scope->getTriggerLevel();
        checkInstrument(scope);
        const double timebase = scope->getTimebase();
        checkInstrument(scope);
        const QString slope = scope->captureSlope();
        checkInstrument(scope);
        if (!std::isfinite(level) || std::abs(level) >= 1e20 || !std::isfinite(timebase) || timebase <= 0 ||
            slope.isEmpty())
            fail("Capture: trigger/timebase readback unavailable");
        const QString id = QString("%1_Cap%2").arg(m_prefix).arg(m_sequence, 4, 10, QChar('0'));
        const QString conditionSuffix = condition.index < 0 ? QString{} : QString("_%1%2").arg(condition.kind == LoadConditionKind::Dynamic ? "DyLoad" : "Load").arg(condition.index + 1);
        const QString stem = QDir(m_directory).filePath(id + conditionSuffix + "_Trig" + QString::number(level, 'g', 9));
        auto number = [](double v) {
            return std::isfinite(v) && std::abs(v) < 1e30 ? QJsonValue(v) : QJsonValue();
        };
        QJsonArray channels;
        for (const auto &value : values) {
            check();
            QString error;
            double offset = std::numeric_limits<double>::quiet_NaN();
            if (!scope->readChannelOffset(value.channel, offset, error))
                fail("Capture Offset readback: " + error);
            const double scale = scope->getChannelScale(value.channel);
            checkInstrument(scope);
            const double position = scope->getChannelPosition(value.channel);
            checkInstrument(scope);
            if (!std::isfinite(offset) || !std::isfinite(scale) || scale <= 0 ||
                !std::isfinite(position))
                fail("Capture: invalid vertical settings");
            channels.append(QJsonObject{{"channel", value.channel},
                                        {"max", number(value.maxVal)},
                                        {"min", number(value.minVal)},
                                        {"rms", number(value.rms)},
                                        {"mean", number(value.mean)},
                                        {"scale", scale},
                                        {"position", position},
                                        {"offset", offset}});
        }
        QStringList saved;
        auto rollback = qScopeGuard([&] {
            for (const auto &path : saved)
                QFile::remove(path);
        });
        if (m_formats.contains("PNG")) {
            check();
            if (!scope->waitForOperationComplete(15000))
                fail("Capture: instrument not ready to save screenshot");
            saved << stem + ".png";
            if (!CaptureFile::screenshot(scope, saved.last(), "PNG"))
                fail("Capture: PNG save failed");
        }
        QSet<QString> transfers;
        for (const auto &format : m_formats) {
            if (format == "PNG")
                continue;
            const bool allChannels = format.startsWith("All");
            const QString kind = allChannels ? format.mid(3) : format;
            for (const auto &value : values) {
                if (!allChannels && value.channel != selected)
                    continue;
                const QString suffix = QString("_CH%1.%2").arg(value.channel).arg(kind.toLower());
                if (transfers.contains(suffix))
                    continue;
                check();
                saved << stem + suffix;
                if (!CaptureFile::waveform(scope, value.channel, saved.last(), kind))
                    fail("Capture: " + suffix + " save failed");
                transfers.insert(suffix);
            }
        }
        check();
        requireStopped(scope);
        const QString metaPath = stem + ".json";
        QJsonObject metadata{{"captureId", id},
                             {"task", m_task},
                             {"utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                             {"triggerSource", source},
                             {"triggerSlope", slope},
                             {"triggerLevel", level},
                             {"timescale", timebase},
                             {"channels", channels},
                             {"files", QJsonArray::fromStringList(saved)},
                             {"newExtrema", QJsonArray::fromStringList(improvements.keys())},
                             {"pngNote", "Screen reference; waveform files are the acquisition data."}};
        if (condition.index >= 0)
            metadata[condition.kind == LoadConditionKind::Dynamic ? "dynamicLoadCondition" : "loadCondition"] = QJsonObject{{"row", condition.index + 1}, {"label", condition.label}};
        saved << metaPath;
        if (!BinaryFileStore::save(metaPath, QJsonDocument(metadata).toJson()).succeeded())
            fail("Capture: metadata save failed");
        check();
        rollback.dismiss();
        for (auto it = improvements.cbegin(); it != improvements.cend(); ++it)
            m_winners[it.key()] = {it.value(), id, level, saved, condition};
    }

    QStringList files() const
    {
        QStringList result;
        for (const auto &winner : m_winners)
            result += winner.files;
        result.removeDuplicates();
        return result;
    }
    QString summary() const
    {
        QStringList result;
        for (auto it = m_winners.cbegin(); it != m_winners.cend(); ++it) {
            result << QString("%1=%2: %3, Trigger=%4")
                          .arg(it.key(), formatOscMeasurement(it->value), it->id,
                               formatOscMeasurement(it->level));
            if (it->condition.index >= 0)
                result.last() += QString(", %1 row %2 (%3)").arg(it->condition.typeName()).arg(it->condition.index + 1).arg(it->condition.label);
        }
        if (!result.isEmpty())
            result << "Capture directory: " + m_directory;
        return result.join('\n');
    }

  private:
    struct Winner {
        double value;
        QString id;
        double level;
        QStringList files;
        Page5ConditionSource condition;
    };
    static void fail(const QString &error) { throw std::runtime_error(error.toStdString()); }
    static void checkInstrument(IScopeCapture *scope)
    {
        if (!scope->lastError().isEmpty())
            fail("Capture readback: " + scope->lastError());
    }
    void check() const
    {
        if (m_stop.loadAcquire())
            fail("Capture: stopped by user");
    }
    static void requireStopped(IScopeCapture *scope)
    {
        if (scope->acquisitionState() != ScopeAcquisitionState::Completed)
            fail("Capture: acquisition is not stopped");
    }
    CaptureOptions m_settings;
    QString m_target, m_task, m_directory, m_prefix;
    QStringList m_formats;
    QAtomicInt &m_stop;
    int m_sequence = 0;
    QMap<QString, Winner> m_winners;
};
