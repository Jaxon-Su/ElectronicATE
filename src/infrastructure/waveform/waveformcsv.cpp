#include "waveformcsv.h"
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <QLocale>

namespace Waveform {
Result exportCsv(const QString &path, const Trace &trace, const Cancellation &cancel)
{
    const auto checked = summarize(trace, cancel);
    if (!checked.ok()) return checked;
    const QFileInfo destination(path), source(trace.sourcePath);
    const auto target = destination.exists() ? destination.canonicalFilePath() : destination.absoluteFilePath();
    const auto origin = source.exists() ? source.canonicalFilePath() : source.absoluteFilePath();
    if (path.isEmpty() || destination.suffix().compare("csv", Qt::CaseInsensitive) != 0 ||
        (!trace.sourcePath.isEmpty() && QDir::cleanPath(target).compare(QDir::cleanPath(origin), Qt::CaseInsensitive) == 0))
        return {{}, QStringLiteral("Choose a CSV destination different from the source WFM.")};
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {{}, file.errorString()};
    QTextStream stream(&file);
    stream.setLocale(QLocale::c());
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    stream.setRealNumberPrecision(17);
    QString unit = trace.unit;
    unit.replace('"', QStringLiteral("\"\""));
    if (trace.minMaxPairs) stream << "Time (s),\"Min (" << unit << ")\",\"Max (" << unit << ")\"\n";
    else stream << "Time (s),\"Value (" << unit << ")\"\n";
    for (qsizetype i = 0; i < trace.samples.size(); i += trace.valuesPerPoint()) {
        if ((i & 4095) == 0 && stopped(cancel)) { file.cancelWriting(); return {{}, {}, true}; }
        stream << trace.timeAt(i) << ',' << trace.samples[i];
        if (trace.minMaxPairs) stream << ',' << trace.samples[i+1];
        stream << '\n';
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || file.error() != QFileDevice::NoError) {
        file.cancelWriting();
        return {{}, QStringLiteral("Could not write the complete CSV file.")};
    }
    if (stopped(cancel)) { file.cancelWriting(); return {{}, {}, true}; }
    if (!file.commit()) return {{}, file.errorString()};
    return {trace, {}};
}
}
