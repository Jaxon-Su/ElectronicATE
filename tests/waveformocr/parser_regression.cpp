#include "service/waveformocr/waveformocrparser.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cmath>
#include <cstdlib>

using namespace WaveformOcr;
int checks = 0;
void require(bool ok, const QString &message)
{
    ++checks;
    if (!ok) {
        QTextStream(stderr) << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
QVector<Row> parse(const QString &text)
{
    return parseText("C:/samples/BD1/1.png", "C:/samples", text);
}
void value(const Row &row, int index, double expected, const QString &unit)
{
    const auto &v = row.values[index];
    require(v.number && std::abs(*v.number - expected) <= std::max(1e-12, std::abs(expected) * 1e-10),
            Metrics[index] + " expected " + QString::number(expected) + ", got " + displayValue(v));
    require(v.unit == unit, "unit " + unit);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString transcription = "Meas 1 RMS\n8.225 A\nMeas 2 Mean\n6.723 A\nMeas 3 Maximum\nµ': 15.56 A\nMeas 4 Minimum\nµ': -185.3 mA";
    const auto wrapper = QString::fromUtf8(QJsonDocument(QJsonObject{{"text", transcription}}).toJson());
    const auto wrapped = parse(wrapper);
    require(wrapped.size() == 1 && wrapped[0].channel.isEmpty(), "unknown channel does not block values");
    require(wrapped[0].rawResponse == wrapper && !wrapped[0].reviewed, "preserve response without claiming human review");
    value(wrapped[0], 0, 15.56, "A");
    value(wrapped[0], 1, -0.1853, "A");
    value(wrapped[0], 2, 8.225, "A");
    value(wrapped[0], 3, 6.723, "A");
    require(issues(wrapped[0]).isEmpty(), "optional metadata is not an error");
    const auto channels = parse("CH1\nMax 1 V\nCH2\nMax 2 V\nCH3\nMax 3 V");
    require(channels.size() == 3, "three channels produce three rows");
    for (int i = 0; i < 3; ++i) {
        require(channels[i].channel == "CH" + QString::number(i + 1), "explicit channel retained");
        value(channels[i], 0, i + 1, "V");
    }
    const QStringList markers{"µ'", "μ¹", "μ′", "µ’", "μ", "µ"};
    for (const auto &marker : markers) {
        const auto rows = parse("Meas 7 2\nMaximum\n" + marker + ": -2.5e-3 A");
        require(rows.size() == 1 && rows[0].channel == "CH2", "badge source, not measurement id");
        value(rows[0], 0, -0.0025, "A");
        require(rows[0].values[0].raw.startsWith(marker), "preserve statistic marker");
        value(parse("CH2\nMaximum " + marker + ": 7.1 mV")[0], 0, 0.0071, "V");
    }
    value(parse("CH1\nRMS\n2.5 µA")[0], 2, 2.5e-6, "A");
    value(parse("CH1\nMinimum: −4 mV")[0], 1, -0.004, "V");
    value(parse("Maximum (CH3):\n7 V")[0], 0, 7, "V");
    value(parse("CH3 Maximum\n7 V")[0], 0, 7, "V");
    auto inlineRows = parse("Meas 3 2 Maximum\n\nµ': 15.56 A\nMeas 4 2 Minimum µ': -185.3 mA");
    require(inlineRows[0].channel == "CH2", "inline badge type with source");
    value(inlineRows[0], 0, 15.56, "A");
    value(inlineRows[0], 1, -0.1853, "A");
    auto repeated = parse("Meas 1 RMS\n3 A\nMeas 1 RMS\n3 A");
    value(repeated[0], 2, 3, "A");

    auto rows = parse("Meas 3 2\nMaximum\n38.06 A\nMeas 1 1\nMaximum\n201.6 V");
    require(rows.size() == 2 && rows[0].channel == "CH2" && rows[1].channel == "CH1", "separate channels");
    value(rows[0], 0, 38.06, "A");
    value(rows[1], 0, 201.6, "V");
    rows = parse("Meas 3 CH2\nMaximum\n38 A\nMeas 1\nMinimum\n-3 A");
    require(rows.size() == 2 && rows[1].channel.isEmpty(), "missing badge source resets channel");
    rows = parse("Meas 3 2\nMaximum\n38 A\nMeas 1 M1\nMinimum\n-3 A");
    require(rows.size() == 2 && rows[1].channel.isEmpty(), "unsupported source never inherits CH2");
    rows = parse("Meas 3 1 2\nMaximum\n38 A");
    require(rows[0].channel.isEmpty(), "ambiguous badge source stays unknown");

    rows = parse("CH1\nMaximum\nMeas 2 1\nMinimum\n-4 V");
    require(!rows[0].values[0].number, "do not cross another badge");
    value(rows[0], 1, -4, "V");
    rows = parse("CH1\nMaximum\nTrigger\n200 V");
    require(!rows[0].values[0].number, "do not search past UI text");
    rows = parse("CH1\nMaximum\n| Unsupported | table |\n200 V");
    require(!rows[0].values[0].number, "do not cross discarded table rows");
    rows = parse("CH1\nMaximum\nMean Min Max\n1 V 2 V 3 V");
    require(!rows[0].values[0].number && !rows[0].values[3].number, "do not interpret statistics header");
    rows = parse("CH1\nMaximum\n5 V 6 V");
    require(!rows[0].values[0].number, "multiple numbers are ambiguous");
    rows = parse("Meas 1 1\nMaximum\n5 V\nMeas 2 1\nMaximum\n6 V");
    require(!rows[0].values[0].number && !rows[0].values[0].issue.isEmpty(), "duplicate candidates rejected");

    rows = parse("| Measurement | Source | Current | Mean | Min | Max |\n"
                 "| --- | --- | --- | --- | --- | --- |\n"
                 "| RMS | CH2 | 3 V | 4 V | 2 V | 5 V |\n");
    require(rows.size() == 1 && rows[0].scope == "current", "existing markdown table scope");
    value(rows[0], 2, 3, "V");
    require(!rows[0].values[0].number && !rows[0].values[1].number && !rows[0].values[3].number,
            "statistics columns do not become measurement types");
    rows = parse("<table><tr><th>Measurement</th><th>Source</th><th>Value</th><th>Unit</th></tr>"
                 "<tr><td>Maximum</td><td>CH1</td><td>7.2</td><td>mV</td></tr></table>");
    value(rows[0], 0, 0.0072, "V");
    rows = parse(R"({"channels":[{"channel":"CH1","scope":"current","max":"5.1 V","min":null,"rms":"3 mV","mean":{"value":2,"unit":"V"}}]})");
    require(rows[0].scope == "current" && rows[0].channel == "CH1", "existing JSON structure");
    value(rows[0], 0, 5.1, "V");
    value(rows[0], 2, 0.003, "V");
    value(rows[0], 3, 2, "V");
    require(!rows[0].values[1].number, "null remains missing");
    rows = parse("CH1\ncurrent acquisition\nMaximum: 5 V\nall acquisitions\nMaximum: 6 V");
    require(rows.size() == 2 && rows[0].scope != rows[1].scope, "separate statistical scopes");
    QTextStream(stdout) << "PASS: " << checks << " regression assertions\n";
}
