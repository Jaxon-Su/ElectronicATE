#include "../../src/infrastructure/waveform/tekwfmreader.h"
#include "../../src/infrastructure/waveform/waveformcsv.h"
#include "../../src/viewmodels/wfmviewer/wfmviewerviewmodel.h"
#include "../../src/views/wfmviewer/waveformplot.h"
#include "../../src/views/wfmviewer/wfmviewerpage.h"
#include "../support/test_evidence.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QWheelEvent>
#include <QPushButton>
#include <QClipboard>
#include <QThreadPool>
#include <QFontDatabase>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace {
QString official(const QString &name)
{
    return QStringLiteral(WFM_FIXTURE_DIR "/") + name;
}
template<class T> void put(QByteArray &bytes, int at, T value, bool little = true)
{
    if (little) qToLittleEndian<T>(value, bytes.data()+at);
    else qToBigEndian<T>(value, bytes.data()+at);
}
void putDouble(QByteArray &b,int at,double value,bool little=true)
{
    quint64 bits; std::memcpy(&bits,&value,8); put(b,at,bits,little);
}
void checksum(QByteArray &bytes,bool little=true)
{
    quint64 sum=0;
    for (int i=0;i<bytes.size()-8;++i) sum+=static_cast<unsigned char>(bytes[i]);
    put(bytes,bytes.size()-8,sum,little);
}
QByteArray fixture(int format=0,bool little=true)
{
    // Packed v3 fields from Tektronix 001-1378-03 and official tm_data_types samples, independently known six-point signal.
    const int sizes[]{2,4,4,8,4,8,1,1}; const int width=sizes[format];
    QByteArray b(838+6*width+8,'\0'); b.replace(0,2,QByteArray::fromHex(little?"0f0f":"f0f0")); b.replace(2,8,":WFM#003");
    b[15]=char(width); put<qint32>(b,16,838,little);
    for (int at:{82,110,114,118,142,760,764}) put<quint32>(b,at,1,little);
    put<quint32>(b,122,2,little); putDouble(b,168,.5,little); putDouble(b,176,-2,little);
    b[188]='V'; put<quint32>(b,240,format,little);
    putDouble(b,488,1,little); putDouble(b,496,-3,little); put<quint32>(b,504,6,little); b[508]='s';
    for(int at:{826,830,834}) put<quint32>(b,at,6*width,little);
    for(int i=0;i<6;++i) {
        const int pos=838+i*width,value=i+1;
        switch(format) {
        case 0: put<qint16>(b,pos,value,little);break;
        case 1: put<qint32>(b,pos,value,little);break;
        case 2: put<quint32>(b,pos,value,little);break;
        case 3: put<quint64>(b,pos,value,little);break;
        case 4: { float f=value+.25f;quint32 bits;std::memcpy(&bits,&f,4);put(b,pos,bits,little);break; }
        case 5: putDouble(b,pos,value+.25,little);break;
        case 6: put<quint8>(b,pos,value,little);break;
        case 7: put<qint8>(b,pos,value,little);break;
        }
    }
    checksum(b,little); return b;
}
Waveform::Result readBytes(const QByteArray &bytes)
{
    QTemporaryDir temp;
    QFile file(temp.filePath("input.wfm"));
    if (!file.open(QIODevice::WriteOnly)) return {{},QStringLiteral("Fixture IO failed")};
    file.write(bytes);file.close();return Waveform::readTekWfm(file.fileName());
}
}

class WfmViewerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
        QApplication::setFont(QFont("Microsoft JhengHei", 10));
    }
    void officialSample()
    {
        const auto r = Waveform::readTekWfm(official("data_test_waveform.wfm"));
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.trace.samples.size(), 1000);
        QCOMPARE(r.trace.unit, QStringLiteral("V"));
        QCOMPARE(r.trace.interval, 4e-11);
        QCOMPARE(r.trace.start, -2e-8);
        QCOMPARE(r.trace.samples.first(), -0.004);
        QCOMPARE(r.trace.samples.last(), -0.008);
        QVERIFY(std::abs(r.trace.statistics.minimum + .252) < 1e-12);
        QVERIFY(std::abs(r.trace.statistics.maximum - .244) < 1e-12);
        QVERIFY(std::abs(r.trace.statistics.mean + .002752) < 1e-12);
        QVERIFY(std::abs(r.trace.statistics.rms - .17540271377604166) < 1e-12);
    }
    void moreOfficialSamples_data()
    {
        QTest::addColumn<QString>("file");QTest::addColumn<int>("points");
        QTest::addColumn<double>("min");QTest::addColumn<double>("max");QTest::addColumn<double>("mean");QTest::addColumn<double>("rms");
        QTest::newRow("analog")<<"analog_waveform.wfm"<<50000<<-.152<<.144<<-.0029832<<.08754839393158506;
        QTest::newRow("golden")<<"golden_analog.wfm"<<6<<.0003051850947599719<<.9834284493545336<<.49186681722464676<<.6953673864688686;
    }
    void moreOfficialSamples()
    {
        QFETCH(QString,file);QFETCH(int,points);QFETCH(double,min);QFETCH(double,max);QFETCH(double,mean);QFETCH(double,rms);
        auto r=Waveform::readTekWfm(official(file));QVERIFY2(r.ok(),qPrintable(r.error));
        QCOMPARE(r.trace.samples.size(),points);
        QVERIFY(std::abs(r.trace.statistics.minimum-min)<1e-12);QVERIFY(std::abs(r.trace.statistics.maximum-max)<1e-12);
        QVERIFY(std::abs(r.trace.statistics.mean-mean)<1e-12);QVERIFY(std::abs(r.trace.statistics.rms-rms)<1e-12);
    }
    void encodings_data()
    {
        QTest::addColumn<int>("format");QTest::addColumn<bool>("little");
        for(int f=0;f<8;++f)for(bool endian:{false,true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(f).arg(endian?"LE":"BE")))<<f<<endian;
    }
    void encodings()
    {
        QFETCH(int,format);QFETCH(bool,little);const auto r=readBytes(fixture(format,little));
        QVERIFY2(r.ok(),qPrintable(r.error));QCOMPARE(r.trace.samples.size(),6);
        const double fraction=(format==4||format==5)?.125:0;
        QCOMPARE(r.trace.samples.first(),-1.5+fraction);QCOMPARE(r.trace.samples.last(),1.+fraction);
        QCOMPARE(r.trace.start,-3.);QCOMPARE(r.trace.end(),2.);
    }
    void rejectsOtherModes()
    {
        for(const auto &name:{"digital_waveform.wfm","iq_waveform.wfm","fastframe/FF5MhzX100From5Series.wfm"})
            QVERIFY2(!Waveform::readTekWfm(official(name)).ok(),name);
    }
    void peakDetect_data()
    {
        QTest::addColumn<bool>("little"); QTest::addColumn<bool>("negativeScale");
        QTest::addColumn<double>("slotInterval"); QTest::addColumn<int>("pairs");
        for (bool little : {false, true}) {
            QTest::newRow(little ? "16us-1.25G-LE" : "16us-1.25G-BE") << little << false << 4e-10 << 200000;
            QTest::newRow(little ? "100us-625K-LE" : "100us-625K-BE") << little << true << 8e-7 << 625;
        }
    }
    void peakDetect()
    {
        QFETCH(bool,little); QFETCH(bool,negativeScale); QFETCH(double,slotInterval); QFETCH(int,pairs);
        auto b=fixture(0,little); b.resize(838+pairs*4+8);
        put<quint32>(b,244,1,little); put<quint32>(b,504,pairs*2,little);
        for (int at : {826,830,834}) put<quint32>(b,at,pairs*4,little);
        putDouble(b,488,slotInterval,little); putDouble(b,496,0,little);
        if (negativeScale) putDouble(b,168,-.5,little);
        for (int i=0;i<pairs;++i) {
            put<qint16>(b,838+i*4,1,little); put<qint16>(b,840+i*4,6,little);
        }
        checksum(b,little);
        const auto r=readBytes(b); QVERIFY2(r.ok(),qPrintable(r.error));
        QCOMPARE(r.trace.samples.size(),pairs*2);
        QCOMPARE(r.trace.timeAt(0),0.); QCOMPARE(r.trace.timeAt(1),0.);
        QCOMPARE(r.trace.timeAt(2),slotInterval*2);
        QCOMPARE(r.trace.end(),double(pairs-1)*2*slotInterval);
        const double low=negativeScale?-5:-1.5, high=negativeScale?-2.5:1;
        QCOMPARE(r.trace.samples[0],low); QCOMPARE(r.trace.samples[1],high);
        const auto preview=WaveformPlot::envelope(r.trace,0,r.trace.end(),100);
        QVERIFY(!preview.isEmpty()); QVERIFY(preview.size()<=200); QCOMPARE(preview.size()%2,0);
        for (int i=0;i<preview.size();i+=2) {
            QCOMPARE(preview[i].x(),preview[i+1].x());
            QCOMPARE(preview[i].y(),low); QCOMPARE(preview[i+1].y(),high);
        }
        QTemporaryDir d; QFile input(d.filePath("peak.wfm")); QVERIFY(input.open(QIODevice::WriteOnly));
        input.write(b); input.close();
        WfmViewerViewModel vm; WfmViewerPage page(&vm); vm.load(input.fileName());
        QTRY_VERIFY(!vm.busy()); QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        const auto *summary=page.findChild<QLabel*>("waveformStatistics"); QVERIFY(summary);
        QVERIFY(summary->text().contains("Pairs:")); QVERIFY(summary->text().contains("Pair interval:"));
        vm.setCursor(1,0); vm.setCursor(2,slotInterval*4); QCOMPARE(vm.deltaTime(),slotInterval*4);
        vm.zoom(.5,slotInterval*4,0); vm.fit(); QCOMPARE(vm.deltaTime(),slotInterval*4);
        vm.exportCsv(d.filePath("peak.csv")); QTRY_VERIFY(!vm.busy()); QVERIFY(vm.error().isEmpty());
        QFile csv(d.filePath("peak.csv")); QVERIFY(csv.open(QIODevice::ReadOnly));
        const auto rows=csv.readAll().trimmed().split('\n'); QCOMPARE(rows.size(),pairs+1);
        QCOMPARE(rows[0],QByteArray("Time (s),\"Min (V)\",\"Max (V)\""));
        QCOMPARE(rows[1].split(',')[1].toDouble(),low); QCOMPARE(rows[1].split(',')[2].toDouble(),high);
        QCOMPARE(rows.last().split(',')[0].toDouble(),r.trace.end());
    }
    void connectedBounds_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<bool>("connected");
        for (const auto *mode : {"peak", "clipped", "single-pair", "flat", "sample"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode) << true;
            if (QString::fromLatin1(mode) != "sample")
                QTest::newRow(qPrintable(QString::fromLatin1(mode)+"-off")) << QString::fromLatin1(mode) << false;
        }
    }
    void connectedBounds()
    {
        QFETCH(QString, mode);
        QFETCH(bool, connected);
        auto bytes = fixture();
        const bool paired = mode != "sample";
        const QVector<qint16> values = mode == "single-pair" ? QVector<qint16>{1,5}
            : mode == "flat" ? QVector<qint16>{1,1,2,2,3,3} : QVector<qint16>{1,5,2,6,3,7};
        bytes.resize(838 + values.size()*2 + 8);
        put<quint32>(bytes,244,paired ? 1 : 0);
        putDouble(bytes,168,1); putDouble(bytes,176,0); putDouble(bytes,496,0);
        put<quint32>(bytes,504,values.size());
        for (int at : {826,830,834}) put<quint32>(bytes,at,values.size()*2);
        for (int i=0; i<values.size(); ++i) put<qint16>(bytes,838+i*2,values[i]);
        checksum(bytes);
        QTemporaryDir directory;
        QFile input(directory.filePath("bounds.wfm")); QVERIFY(input.open(QIODevice::WriteOnly));
        QCOMPARE(input.write(bytes),bytes.size()); input.close();
        WfmViewerViewModel vm; vm.load(input.fileName());
        QTRY_VERIFY(!vm.busy()); QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        const auto original = vm.trace().samples;
        vm.setCursor(1,vm.trace().start); vm.setCursor(2,vm.trace().end());
        vm.setAmplitudeCursor(1,vm.viewRange().minimum); vm.setAmplitudeCursor(2,vm.viewRange().maximum);
        if (mode == "clipped") vm.zoomTo({.5,2.5,1,6});
        WfmViewerPage page(&vm); page.resize(1000,620); page.show();
        auto *option=page.findChild<QCheckBox*>("connectBounds"); QVERIFY(option);
        QVERIFY(!option->isChecked()); QCOMPARE(option->isEnabled(),paired);
        if (paired && connected) QTest::mouseClick(option,Qt::LeftButton);
        auto &plot=*page.findChild<WaveformPlot*>();
        QApplication::processEvents();
        const QImage image = plot.grab().toImage();
        QVERIFY(image.save(TestEvidence::path("connected-bounds-"+mode+(connected?"-on":"-off")+".png")));
        const auto area = plot.plotArea(); const auto range = vm.viewRange();
        const auto waveAt = [&](double time, double value) {
            const int x = qRound(area.left() + (time-range.from)/(range.to-range.from)*area.width());
            const int y = qRound(area.bottom() - (value-range.minimum)/(range.maximum-range.minimum)*area.height());
            for (int dy=-2; dy<=2; ++dy) for (int dx=-2; dx<=2; ++dx) {
                if (!image.rect().contains(x+dx,y+dy)) continue;
                const auto color = image.pixelColor(x+dx,y+dy);
                // Thin lines and zero-height points may cover only part of a pixel. Accept
                // its antialias blend, but exclude the differently colored cursors.
                if (std::abs(color.red()-101)+std::abs(color.green()-186)+std::abs(color.blue()-255)<240
                    && color.blue()>color.green()+25 && color.red()<130 && color.green()>95)
                    return true;
            }
            return false;
        };
        if (mode == "sample") {
            QVERIFY2(waveAt(.5,3),"Sample still connects consecutive samples");
            QVERIFY(!waveAt(1,1.5));
        } else if (mode == "single-pair") {
            // Both time cursors coincide with the span. Check its geometry
            // and find rendered waveform pixels through their dash gaps.
            const auto bounds = WaveformPlot::envelope(vm.trace(),range.from,range.to,100);
            QCOMPARE(bounds,QPolygonF({QPointF(0,1),QPointF(0,5)}));
            bool vertical = false;
            for (int row=0; row<int(area.height()); ++row)
                vertical |= waveAt(0,2+2.*row/area.height());
            QVERIFY2(vertical,"The single pair must render its vertical span");
            QVERIFY(!waveAt(.25,3));
        } else if (mode == "flat") {
            QCOMPARE(waveAt(1,1.5),connected);
            QVERIFY2(waveAt(2,2),"Equal Min/Max remains visible without connections");
        } else {
            QCOMPARE(waveAt(1,1.5),connected);
            QCOMPARE(waveAt(1,5.5),connected);
            QVERIFY2(waveAt(2,4),"Each pair keeps its vertical span");
            QVERIFY2(!waveAt(1,3.5),"No previous-Max to next-Min diagonal");
            if (mode == "clipped") {
                QCOMPARE(waveAt(.52,1.26),connected); QCOMPARE(waveAt(.52,5.26),connected);
                const double outside = range.from - 8*(range.to-range.from)/area.width();
                QVERIFY2(!waveAt(outside,1.25),"Connections stay clipped to the plot");
            }
        }
        QCOMPARE(vm.trace().samples,original);
    }
    void boundsOptionState()
    {
        QTemporaryDir directory; auto bytes=fixture(); put<quint32>(bytes,244,1); checksum(bytes);
        QFile input(directory.filePath("peak.wfm")); QVERIFY(input.open(QIODevice::WriteOnly));
        input.write(bytes); input.close();
        WfmViewerViewModel vm; WfmViewerPage page(&vm); page.resize(1000,620); page.show();
        auto *option=page.findChild<QCheckBox*>("connectBounds"); QVERIFY(option);
        QVERIFY(!option->isEnabled()); QVERIFY(!option->isChecked());
        vm.load(input.fileName()); QTRY_VERIFY(!vm.busy()); QVERIFY(vm.hasTrace());
        QVERIFY(option->isEnabled()); QTest::mouseClick(option,Qt::LeftButton); QVERIFY(option->isChecked());
        const auto original=vm.trace().samples; const auto range=vm.viewRange();
        const auto t1=vm.cursor1(), y1=vm.amplitudeCursor1();
        vm.exportCsv(directory.filePath("on.csv")); QVERIFY(!option->isEnabled()); QTRY_VERIFY(!vm.busy());
        QVERIFY(option->isChecked()); QTest::mouseClick(option,Qt::LeftButton);
        vm.exportCsv(directory.filePath("off.csv")); QTRY_VERIFY(!vm.busy());
        QFile on(directory.filePath("on.csv")),off(directory.filePath("off.csv"));
        QVERIFY(on.open(QIODevice::ReadOnly)); QVERIFY(off.open(QIODevice::ReadOnly)); QCOMPARE(on.readAll(),off.readAll());
        QCOMPARE(vm.trace().samples,original); QCOMPARE(vm.viewRange().from,range.from); QCOMPARE(vm.viewRange().maximum,range.maximum);
        QCOMPARE(vm.cursor1(),t1); QCOMPARE(vm.amplitudeCursor1(),y1);
        QTest::mouseClick(option,Qt::LeftButton); vm.load(directory.filePath("missing.wfm")); QTRY_VERIFY(!vm.busy());
        QVERIFY(option->isChecked()); QVERIFY(option->isEnabled()); QCOMPARE(vm.trace().samples,original);
        vm.load(official("analog_waveform.wfm"));QVERIFY(!option->isEnabled());
        for(const auto *name:{"cursorMode","panMode","zoomMode"}) QVERIFY(!page.findChild<QPushButton*>(name)->isEnabled());
        vm.cancel();QTRY_VERIFY(!vm.busy());QVERIFY(vm.status().contains("Cancelled"));
        QVERIFY(option->isChecked());QVERIFY(option->isEnabled());QCOMPARE(vm.trace().samples,original);
        QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.amplitudeCursor1(),y1);
        vm.load(input.fileName()); QTRY_VERIFY(!vm.busy()); QVERIFY(!option->isChecked());
        QTest::mouseClick(option,Qt::LeftButton);
        vm.load(official("data_test_waveform.wfm")); QTRY_VERIFY(!vm.busy());
        QVERIFY(!option->isChecked()); QVERIFY(!option->isEnabled());
    }
    void peakEnvelopeExtrema()
    {
        Waveform::Trace trace; trace.minMaxPairs=true; trace.interval=.5;
        for (int i=0;i<2000;++i) { trace.samples.append(-1); trace.samples.append(2); }
        trace.samples[15*2]=-30; trace.samples[55*2+1]=90;
        const auto overview=WaveformPlot::envelope(trace,0,1999,20);
        QCOMPARE(overview.size(),40);
        QCOMPARE(overview[0],QPointF(0,-30)); QCOMPARE(overview[1],QPointF(0,90));
        QCOMPARE(overview[2],QPointF(100,-1)); QCOMPARE(overview[3],QPointF(100,2));
        const auto zoomed=WaveformPlot::envelope(trace,14,16,100);
        QCOMPARE(zoomed,QPolygonF({{14,-1},{14,2},{15,-30},{15,2},{16,-1},{16,2}}));
    }
    void peakDetectInvalid()
    {
        auto b=fixture(); put<quint32>(b,244,1); checksum(b);
        QVERIFY2(readBytes(b).ok(),"Valid Min/Max control must load");
        auto odd=b; put<quint32>(odd,826,10); put<quint32>(odd,504,5); checksum(odd);
        QVERIFY(!readBytes(odd).ok());
        auto misaligned=b; put<quint32>(misaligned,822,2); put<quint32>(misaligned,826,10);
        put<quint32>(misaligned,504,4); checksum(misaligned); QVERIFY(!readBytes(misaligned).ok());
        auto reversed=b; put<qint16>(reversed,838,9); checksum(reversed); QVERIFY(!readBytes(reversed).ok());
        auto histogram=b; put<quint32>(histogram,244,2); checksum(histogram); QVERIFY(!readBytes(histogram).ok());
        auto summary=b; put<quint16>(summary,154,2); checksum(summary); QVERIFY(!readBytes(summary).ok());
    }
    void headerTimeScales()
    {
        // Header-driven, not tied to the two instrument configurations reported.
        for (bool pairs : {false,true}) for (bool little : {false,true}) {
            for (int exponent=-15;exponent<=3;exponent+=3) {
                const double dt=std::pow(10.,exponent), start=-20*dt;
                auto b=fixture(0,little); putDouble(b,488,dt,little); putDouble(b,496,start,little);
                put<quint32>(b,244,pairs?1:0,little); checksum(b,little);
                const auto r=readBytes(b); QVERIFY2(r.ok(),qPrintable(r.error));
                QCOMPARE(r.trace.start,start); QCOMPARE(r.trace.interval,dt);
                const double step=dt*(pairs?2:1);
                QCOMPARE(r.trace.timeAt(pairs?2:1),start+step);
                QCOMPARE(r.trace.end(),start+step*(pairs?2:5));
            }
        }
    }
    void realMetadata_data()
    {
        QTest::addColumn<bool>("little");
        QTest::newRow("LE")<<true; QTest::newRow("BE")<<false;
    }
    void realMetadata()
    {
        QFETCH(bool,little);
        auto make=[&](QByteArray key) {
            auto b=fixture(0,little); const int pos=b.size();
            b.append("tekmeta!"); b.append(QByteArray(4+4+key.size()+1+4,'\0'));
            put<quint32>(b,pos+8,1,little); put<quint32>(b,pos+12,key.size(),little);
            b.replace(pos+16,key.size(),key); b[pos+16+key.size()]=char(1);
            return b;
        };
        auto b=make("ANALOG_Thumbnail"); QVERIFY2(readBytes(b).ok(),"Metadata control must load");
        b.append('\n'); QVERIFY(readBytes(b).ok()); b.append('\n'); QVERIFY(!readBytes(b).ok());
        QVERIFY(!readBytes(make("IQ_sampleRate")).ok()); QVERIFY(!readBytes(make("d0")).ok());
        b=make("ANALOG_Thumbnail"); b.chop(1); QVERIFY(!readBytes(b).ok());
        b=make("ANALOG_Thumbnail"); b.append('x'); QVERIFY(!readBytes(b).ok());
    }
    void corruptHeaders_data()
    {
        QTest::addColumn<int>("offset");QTest::addColumn<quint32>("value");
        for (const auto pair : {std::pair<int,quint32>{16,100}, {72,1},{78,1},{82,2},{110,2},{114,2},{118,2},
                               {122,6},{142,2},{146,1},{150,1},{156,2},{240,9},{244,2},{504,10000001},
                               {760,0},{764,0},{768,1},{812,1},{818,20},{822,20},{826,1},{830,10},{834,15}})
            QTest::newRow(qPrintable(QString::number(pair.first)))<<pair.first<<pair.second;
    }
    void corruptHeaders()
    {
        QFETCH(int,offset);QFETCH(quint32,value);auto b=fixture();put(b,offset,value);checksum(b);
        QVERIFY(!readBytes(b).ok());
    }
    void nonfiniteAndPrecision()
    {
        auto b=fixture();putDouble(b,488,0);checksum(b);QVERIFY(!readBytes(b).ok());
        b=fixture();putDouble(b,496,std::numeric_limits<double>::infinity());checksum(b);QVERIFY(!readBytes(b).ok());
        b=fixture(5);putDouble(b,838,std::numeric_limits<double>::quiet_NaN());checksum(b);QVERIFY(!readBytes(b).ok());
        b=fixture(3);put<quint64>(b,838,(quint64(1)<<53)+1);checksum(b);QVERIFY(!readBytes(b).ok());
        b=fixture();b[839]=char(b[839]+1);QVERIFY(readBytes(b).error.contains("checksum"));
        b=fixture();b[9]='2';QVERIFY(readBytes(b).error.contains("version"));
    }
    void metadata()
    {
        auto appendMeta=[](QByteArray b,QByteArray key,quint8 type) {
            const int pos=b.size();b.append(QByteArray(8+4+4+key.size()+1+4,'\0'));
            b.replace(pos,8,QByteArray("tekmeta!\0",8));put<quint32>(b,pos+8,1);put<quint32>(b,pos+12,key.size());
            b.replace(pos+16,key.size(),key);b[pos+16+key.size()]=char(type);return b;
        };
        QVERIFY(readBytes(appendMeta(fixture(),"yPosition",4)).ok());
        QVERIFY(readBytes(appendMeta(fixture(),"d1custom",4)).ok());
        QVERIFY(!readBytes(appendMeta(fixture(),"IQ_sampleRate",4)).ok());
        QVERIFY(!readBytes(appendMeta(fixture(),"d0",4)).ok());
        QVERIFY(!readBytes(appendMeta(fixture(),"anything",9)).ok());
        auto b=appendMeta(fixture(),"yPosition",4);b.chop(1);QVERIFY(!readBytes(b).ok());
        b=fixture();b.append("garbage");QVERIFY(!readBytes(b).ok());
    }
    void cancelled()
    {
        auto cancel = std::make_shared<std::atomic_bool>(true);
        Waveform::Trace t; t.samples = {1,2,3};
        QVERIFY(Waveform::summarize(t,cancel).cancelled);
        QVERIFY(Waveform::readTekWfm(QStringLiteral("missing.wfm"),cancel).cancelled);
    }
    void malformed()
    {
        QTemporaryDir d; QVERIFY(d.isValid());
        QFile f(d.filePath("bad.wfm")); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("bad"); f.close();
        QVERIFY(!Waveform::readTekWfm(f.fileName()).ok());
        QVERIFY(!Waveform::readTekWfm(d.filePath("missing.wfm")).ok());
    }
    void statistics()
    {
        Waveform::Trace t;t.samples={-2,0,2};t.interval=.1;
        auto r=Waveform::summarize(t);QVERIFY(r.ok());QCOMPARE(r.trace.samples,t.samples);
        QCOMPARE(r.trace.statistics.mean,0.);QVERIFY(std::abs(r.trace.statistics.rms-std::sqrt(8./3))<1e-12);
        t.samples={1e300,-1e300};r=Waveform::summarize(t);QVERIFY(r.ok());QCOMPARE(r.trace.statistics.rms,1e300);
        t.samples={3};r=Waveform::summarize(t);QVERIFY(r.ok());QCOMPARE(r.trace.statistics.rms,3.);
        t.samples={std::numeric_limits<double>::quiet_NaN()};QVERIFY(!Waveform::summarize(t).ok());
    }
    void csv()
    {
        QTemporaryDir temp;QVERIFY(temp.isValid());
        Waveform::Trace t;t.start=-.5;t.interval=.25;t.unit="V";t.samples={1.125,-2.5,3};
        const auto path=temp.filePath("processed.csv");
        auto r=Waveform::exportCsv(path,t);QVERIFY2(r.ok(),qPrintable(r.error));
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));const auto before=f.readAll();f.close();
        QCOMPARE(before,QByteArray("Time (s),\"Value (V)\"\n-0.5,1.125\n-0.25,-2.5\n0,3\n"));
        t.samples={100};r=Waveform::exportCsv(path,t,std::make_shared<std::atomic_bool>(true));QVERIFY(r.cancelled);
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),before);f.close();
        t.sourcePath=path;QVERIFY(!Waveform::exportCsv(path,t).ok());
        QVERIFY(!Waveform::exportCsv(temp.filePath("input.wfm"),t).ok());
        QVERIFY(!Waveform::exportCsv(temp.filePath("absent/out.csv"),t).ok());
    }
    void spikeEnvelope()
    {
        Waveform::Trace t;t.samples.fill(0,100000);t.samples[12345]=99;t.samples[54321]=-77;
        const auto points=WaveformPlot::envelope(t,0,99999,100);
        QVERIFY(points.size()<=202);bool high=false,low=false;
        for(const auto &p:points){high|=p.y()==99;low|=p.y()==-77;}
        QVERIFY(high);QVERIFY(low);
        QCOMPARE(points.first(),QPointF(0,0));QCOMPARE(points.last(),QPointF(99999,0));
        const auto zoom=WaveformPlot::envelope(t,12340,12350,100);QVERIFY(zoom.size()>=11);
    }
    void viewModelLifecycle()
    {
        WfmViewerViewModel vm;
        vm.load(official("data_test_waveform.wfm"));QVERIFY(vm.busy());QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        const auto original=vm.trace().samples;
        vm.zoom(.5,0,0);vm.setCursor(1,-1e-9);vm.setAmplitudeCursor(1,-.1);vm.setAmplitudeCursor(2,.1);
        const auto view=vm.viewRange();const auto cursor=vm.cursor1();
        vm.load("missing.wfm");QTRY_VERIFY(!vm.busy());QVERIFY(!vm.error().isEmpty());QCOMPARE(vm.trace().samples,original);
        QCOMPARE(vm.viewRange().from,view.from);QCOMPARE(vm.cursor1(),cursor);QCOMPARE(vm.amplitudeCursor1(),-.1);QCOMPARE(vm.amplitudeCursor2(),.1);
        vm.load("");QVERIFY(!vm.busy());QCOMPARE(vm.trace().samples,original);
        vm.load(official("analog_waveform.wfm"));vm.cancel();QTRY_VERIFY(!vm.busy());
        QCOMPARE(vm.trace().samples,original);QVERIFY(vm.status().contains("Cancelled"));
        QCOMPARE(vm.viewRange().from,view.from);QCOMPARE(vm.cursor1(),cursor);QCOMPARE(vm.amplitudeCursor1(),-.1);QCOMPARE(vm.amplitudeCursor2(),.1);
        QTemporaryDir d;const auto path=d.filePath("result.csv");
        vm.exportCsv(path);QTRY_VERIFY(!vm.busy());QVERIFY(vm.error().isEmpty());
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto lines=file.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(),1001);QCOMPARE(lines[1].split(',')[0].toDouble(),vm.trace().start);QCOMPARE(lines.last().split(',')[0].toDouble(),vm.trace().end());QCOMPARE(lines[1].split(',')[1].toDouble(),-.004);QCOMPARE(lines.last().split(',')[1].toDouble(),-.008);
        QCOMPARE(vm.trace().samples,original);QCOMPARE(vm.viewRange().from,view.from);QCOMPARE(vm.cursor1(),cursor);QCOMPARE(vm.amplitudeCursor1(),-.1);QCOMPARE(vm.amplitudeCursor2(),.1);
        QVERIFY(vm.defaultExportPath().endsWith("data_test_waveform.csv"));
        vm.load(official("analog_waveform.wfm"));vm.load(official("golden_analog.wfm"));
        const auto busyCursor=vm.cursor1();vm.fit();vm.setCursor(1,123);vm.setAmplitudeCursor(1,123);QCOMPARE(vm.cursor1(),busyCursor);QCOMPARE(vm.amplitudeCursor1(),-.1);
        QTRY_VERIFY(!vm.busy());QCOMPARE(vm.trace().samples.size(),50000);
        QCOMPARE(vm.viewRange().from,vm.trace().start);QCOMPARE(vm.viewRange().to,vm.trace().end());
        const auto fresh=vm.viewRange();QVERIFY(std::abs(vm.amplitudeCursor1()-(fresh.minimum+(fresh.maximum-fresh.minimum)*.25))<1e-14);
        auto *closing=new WfmViewerViewModel;closing->load(official("analog_waveform.wfm"));delete closing;
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));QCoreApplication::processEvents();
    }
    void viewAndCursorRules()
    {
        QTemporaryDir d;QFile file(d.filePath("six.wfm"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(fixture());file.close();
        WfmViewerViewModel vm;vm.load(file.fileName());QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        QCOMPARE(vm.cursor1(),-1.75);QCOMPARE(vm.cursor2(),.75);QCOMPARE(vm.deltaTime(),2.5);
        vm.setCursor(1,1);vm.setCursor(2,-1);QCOMPARE(vm.deltaTime(),2.);
        vm.setCursor(2,1);QCOMPARE(vm.deltaTime(),0.);
        vm.setCursor(1,-100);vm.setCursor(2,100);QCOMPARE(vm.cursor1(),-3.);QCOMPARE(vm.cursor2(),2.);
        vm.setCursor(1,std::numeric_limits<double>::quiet_NaN());vm.setCursor(0,0);QCOMPARE(vm.cursor1(),-3.);
        const auto full=vm.viewRange();vm.zoomTo({-2,0,-.5,.5});
        QCOMPARE(vm.viewRange().from,-2.);QCOMPARE(vm.viewRange().to,0.);QCOMPARE(vm.viewRange().minimum,-.5);
        QCOMPARE(vm.cursor1(),-3.);QCOMPARE(vm.cursor2(),2.);
        vm.pan(.5,.1);QCOMPARE(vm.viewRange().from,-1.5);QVERIFY(std::abs(vm.viewRange().minimum+.4)<1e-12);
        vm.zoom(2,-.5,.1);QCOMPARE(vm.viewRange().to-vm.viewRange().from,4.);
        vm.fit();QCOMPARE(vm.viewRange().from,full.from);QCOMPARE(vm.viewRange().maximum,full.maximum);
        QCOMPARE(vm.cursor1(),-3.);QCOMPARE(vm.cursor2(),2.);
        vm.zoomTo({-1,0,-.1,.1});vm.pan(100,100);QCOMPARE(vm.viewRange().to,2.);QCOMPARE(vm.viewRange().maximum,full.maximum);
        const auto valid=vm.viewRange();vm.zoom(-1,0,0);vm.zoomTo({0,0,0,0});vm.pan(NAN,0);QCOMPARE(vm.viewRange().from,valid.from);
        vm.zoom(1e-20,1,0);QVERIFY(vm.viewRange().to-vm.viewRange().from>=1.);
        vm.load(file.fileName());QTRY_VERIFY(!vm.busy());QCOMPARE(vm.cursor1(),-1.75);QCOMPARE(vm.cursor2(),.75);
    }
    void singlePoint()
    {
        auto bytes=fixture();bytes.resize(838+2+8);put<quint32>(bytes,504,1);
        for(int at:{826,830,834})put<quint32>(bytes,at,2);checksum(bytes);
        QTemporaryDir d;QFile file(d.filePath("one.wfm"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();
        WfmViewerViewModel vm;vm.load(file.fileName());QTRY_VERIFY(!vm.busy());QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        QCOMPARE(vm.trace().samples.size(),1);QCOMPARE(vm.cursor1(),-3.);QCOMPARE(vm.cursor2(),-3.);QCOMPARE(vm.deltaTime(),0.);
        QVERIFY(vm.viewRange().from<vm.viewRange().to);QVERIFY(vm.viewRange().minimum<vm.viewRange().maximum);
        vm.zoom(.1,-3,-1.5);QCOMPARE(vm.deltaTime(),0.);vm.setCursor(1,30);QCOMPARE(vm.cursor1(),-3.);
        QVERIFY(vm.amplitudeCursor1()<vm.amplitudeCursor2());QVERIFY(vm.deltaAmplitude()>0);
        vm.setAmplitudeCursor(1,vm.amplitudeCursor2());QCOMPARE(vm.deltaAmplitude(),0.L);
        WaveformPlot plot(&vm);plot.resize(700,400);QVERIFY(!plot.grab().isNull());
    }
    void timeUnits_data()
    {
        QTest::addColumn<double>("value");QTest::addColumn<QString>("text");
        QTest::newRow("seconds")<<2.5<<"2.5 s";QTest::newRow("zero")<<0.<<"0 s";
        QTest::newRow("ms")<<.002<<"2 ms";QTest::newRow("us")<<-3e-6<<"-3 µs";
        QTest::newRow("ns")<<4e-9<<"4 ns";QTest::newRow("ps")<<5e-12<<"5 ps";
    }
    void timeUnits() { QFETCH(double,value);QFETCH(QString,text);QCOMPARE(WfmViewerViewModel::formatTime(value),text); }
    void plotGestures()
    {
        WfmViewerViewModel vm;WaveformPlot plot(&vm);plot.resize(800,500);plot.show();
        QTest::mouseClick(&plot,Qt::LeftButton,Qt::NoModifier,QPoint(200,200));QVERIFY(!vm.hasTrace());
        vm.load(official("data_test_waveform.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        const auto full=vm.viewRange();const double span=full.to-full.from,amp=full.maximum-full.minimum;
        const auto area=plot.plotArea();
        const auto pixel=[&](double x,double y) {return QPoint(qRound(area.left()+area.width()*x),qRound(area.top()+area.height()*y));};
        const auto drag=[&](QPoint from,QPoint to) {QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(&plot,to);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,to);};
        drag(pixel(.2,.2),pixel(.8,.8));const auto selected=vm.viewRange();
        QVERIFY(std::abs((selected.to-selected.from)/span-.6)<.01);QVERIFY(std::abs((selected.maximum-selected.minimum)/amp-.6)<.01);
        vm.fit();drag(pixel(.8,.8),pixel(.2,.2));QCOMPARE(vm.viewRange().from,selected.from);QCOMPARE(vm.viewRange().maximum,selected.maximum);
        drag(pixel(.5,.5),pixel(.5,.5)+QPoint(2,2));QCOMPARE(vm.viewRange().from,selected.from);
        plot.setInteractionMode(WaveformPlot::InteractionMode::Pan);drag(pixel(.5,.5),pixel(.6,.6));
        QVERIFY(vm.viewRange().from<selected.from);QVERIFY(vm.viewRange().minimum>selected.minimum);
        vm.fit();plot.setInteractionMode(WaveformPlot::InteractionMode::Cursors);
        drag(pixel(.25,.5),pixel(.4,.5));QVERIFY(std::abs((vm.cursor1()-full.from)/span-.4)<.003);
        drag(pixel(.75,.5),pixel(.6,.5));QVERIFY(std::abs(vm.deltaTime()/span-.2)<.003);
        const double t1=vm.cursor1(),t2=vm.cursor2();const auto center=pixel(.5,.5);
        QWheelEvent wheel(center,plot.mapToGlobal(center),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&plot,&wheel);QVERIFY(std::abs((vm.viewRange().to-vm.viewRange().from)/span-.8)<1e-12);
        QVERIFY(std::abs((vm.viewRange().maximum-vm.viewRange().minimum)/amp-.8)<1e-12);QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.cursor2(),t2);
        const auto zoomed=vm.viewRange();
        const double cursorFraction=(t1-zoomed.from)/(zoomed.to-zoomed.from);
        drag(pixel(cursorFraction,.5),pixel(.45,.5));
        QVERIFY(std::abs((vm.cursor1()-zoomed.from)/(zoomed.to-zoomed.from)-.45)<.003);
        const double afterDrag=vm.cursor1();
        QTest::mouseDClick(&plot,Qt::LeftButton,Qt::NoModifier,center);QCOMPARE(vm.viewRange().from,full.from);QCOMPARE(vm.cursor1(),afterDrag);
        vm.zoom(.5,0,0);
        const double beforeDouble1=vm.cursor1(),beforeDouble2=vm.cursor2();
        // A real double-click includes the first click; QWidget mouseDClick alone omits it.
        QTest::mouseClick(&plot,Qt::LeftButton,Qt::NoModifier,center);
        QTest::mouseDClick(&plot,Qt::LeftButton,Qt::NoModifier,center);
        QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,center);
        QCOMPARE(vm.viewRange().from,full.from);QCOMPARE(vm.cursor1(),beforeDouble1);QCOMPARE(vm.cursor2(),beforeDouble2);
        plot.setInteractionMode(WaveformPlot::InteractionMode::ZoomRegion);
        QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.2,.2));plot.setInteractionMode(WaveformPlot::InteractionMode::Pan);
        QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.8,.8));QCOMPARE(vm.viewRange().from,full.from);
    }
    void pageControls()
    {
        WfmViewerViewModel vm;WfmViewerPage page(&vm);page.resize(1200,720);page.show();
        auto *fit=page.findChild<QPushButton*>("fitWaveform");
        auto *cursor=page.findChild<QPushButton*>("cursorMode"), *pan=page.findChild<QPushButton*>("panMode"), *zoom=page.findChild<QPushButton*>("zoomMode");
        QVERIFY(cursor);QVERIFY(pan);QVERIFY(zoom);QVERIFY(zoom->isChecked());
        QVERIFY(!page.findChild<QComboBox*>("interactionMode"));
        QVERIFY(!page.findChild<QComboBox*>("cursorAxis"));
        QVERIFY(!fit->isEnabled());for(auto *b:{cursor,pan,zoom}) QVERIFY(!b->isEnabled());
        vm.load(official("data_test_waveform.wfm"));QVERIFY(!page.findChild<QPushButton*>("openWfm")->isEnabled());
        QTRY_VERIFY(!vm.busy());QVERIFY(fit->isEnabled());
        for(auto *selected:{cursor,pan,zoom,cursor}) {
            QVERIFY(selected->isEnabled());QTest::mouseClick(selected,Qt::LeftButton);
            QTest::mouseClick(selected,Qt::LeftButton);
            for(auto *b:{cursor,pan,zoom}) QCOMPARE(b->isChecked(),b==selected);
        }
        auto *stats=page.findChild<QLabel*>("waveformStatistics");QVERIFY(stats->text().contains("Points: 1000"));
        vm.setCursor(1,vm.trace().start);vm.zoom(.1,0,0);QVERIFY(page.findChild<QLabel*>("cursor1Readout")->text().contains("outside view"));
        const double t1=vm.cursor1();QTest::mouseClick(fit,Qt::LeftButton);QCOMPARE(vm.cursor1(),t1);
        QVERIFY(!page.findChild<QLabel*>("cursor1Readout")->text().contains("outside view"));
        QVERIFY(page.findChild<QLabel*>("deltaTimeReadout")->text().contains("ns"));
        const auto path=TestEvidence::path("wfmviewer/viewer-standalone.png");
        QVERIFY(page.grab().save(path));
    }
    void amplitudeCursorControls()
    {
        WfmViewerViewModel vm;WfmViewerPage page(&vm);page.resize(1200,760);page.show();
        QVERIFY(!page.findChild<QComboBox*>("cursorAxis"));
        auto *cursor=page.findChild<QPushButton*>("cursorMode");QVERIFY(cursor);QVERIFY(!cursor->isEnabled());
        QVERIFY(page.findChild<QLabel*>("amplitudeCursor1Readout"));QVERIFY(page.findChild<QLabel*>("deltaAmplitudeReadout"));
        QVERIFY(!page.findChild<QPushButton*>("zoomOut"));
        vm.load(official("data_test_waveform.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(cursor->isEnabled());
        QTest::mouseClick(cursor,Qt::LeftButton);
        vm.setAmplitudeCursor(1,-.2);vm.setAmplitudeCursor(2,.2);
        QCOMPARE(page.findChild<QLabel*>("deltaAmplitudeReadout")->text(),QString::fromUtf8("ΔY: 0.4 V"));
        vm.zoom(.2,0,0);QVERIFY(page.findChild<QLabel*>("amplitudeCursor1Readout")->text().contains("outside view"));
        QTest::mouseClick(page.findChild<QPushButton*>("fitWaveform"),Qt::LeftButton);
        QVERIFY(!page.findChild<QLabel*>("amplitudeCursor1Readout")->text().contains("outside view"));
        QTest::mouseClick(page.findChild<QPushButton*>("panMode"),Qt::LeftButton);QVERIFY(!cursor->isChecked());
        QTest::mouseClick(cursor,Qt::LeftButton);QVERIFY(cursor->isChecked());
        QVERIFY(page.grab().save(TestEvidence::path("wfmviewer/viewer-amplitude.png")));
    }
    void amplitudeRules_data()
    {
        QTest::addColumn<bool>("constant");QTest::addColumn<QString>("unit");
        QTest::newRow("volts")<<false<<"V";QTest::newRow("amps")<<false<<"A";QTest::newRow("constant")<<true<<"V";
    }
    void amplitudeRules()
    {
        QFETCH(bool,constant);QFETCH(QString,unit);
        auto bytes=fixture();bytes[188]=unit[0].toLatin1();if(constant)for(int i=0;i<6;++i)put<qint16>(bytes,838+i*2,3);checksum(bytes);
        QTemporaryDir d;QFile f(d.filePath("input.wfm"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(bytes);f.close();
        WfmViewerViewModel vm;vm.load(f.fileName());QTRY_VERIFY(!vm.busy());QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        const auto full=vm.viewRange();const double height=full.maximum-full.minimum;
        QVERIFY(height>0);QVERIFY(std::abs(vm.amplitudeCursor1()-(full.minimum+height*.25))<1e-14);
        QVERIFY(std::abs(vm.amplitudeCursor2()-(full.minimum+height*.75))<1e-14);
        vm.setAmplitudeCursor(1,full.maximum);vm.setAmplitudeCursor(2,full.minimum);
        QVERIFY(std::abs(vm.deltaAmplitude()-height)<1e-14);QVERIFY(vm.formatAmplitude(vm.deltaAmplitude(),unit).endsWith(unit));
        vm.setAmplitudeCursor(2,full.maximum);QCOMPARE(vm.deltaAmplitude(),0.L);
        vm.setAmplitudeCursor(1,-100);vm.setAmplitudeCursor(2,100);QCOMPARE(vm.amplitudeCursor1(),full.minimum);QCOMPARE(vm.amplitudeCursor2(),full.maximum);
        vm.setAmplitudeCursor(1,NAN);vm.setAmplitudeCursor(0,0);QCOMPARE(vm.amplitudeCursor1(),full.minimum);
        vm.zoom(.5,-.5,full.minimum+height*.5);vm.pan(.1,.001);vm.fit();QCOMPARE(vm.amplitudeCursor1(),full.minimum);QCOMPARE(vm.amplitudeCursor2(),full.maximum);
        vm.zoom(.5,-.5,full.minimum+height*.5);QCOMPARE(vm.amplitudeCursor1(),full.minimum);
        QTemporaryDir output;const auto csv=output.filePath("full.csv");vm.exportCsv(csv);QTRY_VERIFY(!vm.busy());QVERIFY(vm.error().isEmpty());
        QFile saved(csv);QVERIFY(saved.open(QIODevice::ReadOnly));const auto lines=saved.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(),7);QCOMPARE(lines.first(),QByteArray("Time (s),\"Value (")+unit.toLatin1()+")\"");
        QCOMPARE(lines[1].split(',')[0].toDouble(),-3.);QCOMPARE(lines.last().split(',')[0].toDouble(),2.);
        QCOMPARE(lines[1].split(',')[1].toDouble(),constant?-.5:-1.5);QCOMPARE(lines.last().split(',')[1].toDouble(),constant?-.5:1.);
    }
    void amplitudeExtremeDifference()
    {
        auto bytes=fixture(5);putDouble(bytes,168,1);putDouble(bytes,176,0);
        for(int i=0;i<6;++i)putDouble(bytes,838+i*8,i%2?1e308:-1e308);checksum(bytes);
        QTemporaryDir d;QFile f(d.filePath("extreme.wfm"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(bytes);f.close();
        WfmViewerViewModel vm;vm.load(f.fileName());QTRY_VERIFY(!vm.busy());QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        vm.setAmplitudeCursor(1,-1e308);vm.setAmplitudeCursor(2,1e308);
        QVERIFY(std::isfinite(vm.deltaAmplitude()));QVERIFY(vm.deltaAmplitude()>std::numeric_limits<double>::max());
        QCOMPARE(vm.formatAmplitude(vm.deltaAmplitude(),"V"),QStringLiteral("2e+308 V"));
    }
    void amplitudeGestures()
    {
        WfmViewerViewModel vm;WaveformPlot plot(&vm);plot.resize(800,500);plot.show();
        vm.load(official("data_test_waveform.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        const auto area=plot.plotArea(), full=QRectF(QPointF(vm.viewRange().from,vm.viewRange().minimum),QPointF(vm.viewRange().to,vm.viewRange().maximum));
        const auto pixel=[&](double x,double y){return QPoint(qRound(area.left()+x*area.width()),qRound(area.top()+y*area.height()));};
        const auto drag=[&](QPoint from,QPoint to){QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(&plot,to);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,to);};
        plot.setInteractionMode(WaveformPlot::InteractionMode::Cursors);
        const double t1=vm.cursor1(),t2=vm.cursor2();
        drag(pixel(.5,.75),pixel(.5,.6));drag(pixel(.5,.25),pixel(.5,.4));
        QVERIFY(std::abs((vm.amplitudeCursor1()-full.top())/full.height()-.4)<.003);
        QVERIFY(std::abs(vm.deltaAmplitude()/full.height()-.2)<.003);QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.cursor2(),t2);
        vm.zoom(.5,0,0);const auto range=vm.viewRange();const double height=range.maximum-range.minimum;
        const auto fraction=(range.maximum-vm.amplitudeCursor1())/height;
        drag(pixel(.5,fraction),pixel(.5,.55));QVERIFY(std::abs((vm.amplitudeCursor1()-range.minimum)/height-.45)<.003);
        const auto before1=vm.amplitudeCursor1(),before2=vm.amplitudeCursor2();const auto center=pixel(.5,.5);
        QTest::mouseClick(&plot,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseDClick(&plot,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,center);
        QCOMPARE(vm.amplitudeCursor1(),before1);QCOMPARE(vm.amplitudeCursor2(),before2);QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.cursor2(),t2);QCOMPARE(vm.viewRange().from,full.left());
        QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.3,.6));const double selected=vm.amplitudeCursor1();
        plot.setInteractionMode(WaveformPlot::InteractionMode::Pan);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.8,.2));
        QCOMPARE(vm.amplitudeCursor1(),selected);QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.cursor2(),t2);
        QWheelEvent in(center,plot.mapToGlobal(center),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&plot,&in);QVERIFY(vm.viewRange().to-vm.viewRange().from<full.width());
        QWheelEvent out(center,plot.mapToGlobal(center),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&plot,&out);QVERIFY(std::abs((vm.viewRange().to-vm.viewRange().from)/full.width()-1)<1e-12);
        QCOMPARE(vm.amplitudeCursor1(),selected);QCOMPARE(vm.cursor1(),t1);
    }
    void directCursors_data()
    {
        QTest::addColumn<QPointF>("start");QTest::addColumn<int>("timeCursor");QTest::addColumn<int>("amplitudeCursor");
        QTest::newRow("t1")<<QPointF(.25,.5)<<1<<0;
        QTest::newRow("t2")<<QPointF(.75,.5)<<2<<0;
        QTest::newRow("y1")<<QPointF(.5,.75)<<0<<1;
        QTest::newRow("y2")<<QPointF(.5,.25)<<0<<2;
        QTest::newRow("t1-y1")<<QPointF(.25,.75)<<1<<1;
        QTest::newRow("t1-y2")<<QPointF(.25,.25)<<1<<2;
        QTest::newRow("t2-y1")<<QPointF(.75,.75)<<2<<1;
        QTest::newRow("t2-y2")<<QPointF(.75,.25)<<2<<2;
        QTest::newRow("empty-space")<<QPointF(.5,.5)<<0<<0;
    }
    void directCursors()
    {
        QFETCH(QPointF,start);QFETCH(int,timeCursor);QFETCH(int,amplitudeCursor);
        WfmViewerViewModel vm;WfmViewerPage page(&vm);page.resize(1000,620);page.show();
        vm.load(official("data_test_waveform.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        QTest::mouseClick(page.findChild<QPushButton*>("cursorMode"),Qt::LeftButton);
        auto *plot=page.findChild<WaveformPlot*>();const auto area=plot->plotArea();const auto r=vm.viewRange();
        const auto pixel=[&](QPointF p){return QPoint(qRound(area.left()+p.x()*area.width()),qRound(area.top()+p.y()*area.height()));};
        const double times[]{vm.cursor1(),vm.cursor2()},amps[]{vm.amplitudeCursor1(),vm.amplitudeCursor2()};
        const auto destination=pixel({.9,.9});
        QTest::mousePress(plot,Qt::LeftButton,Qt::NoModifier,pixel(start));
        // Cross the other cursor and keep dragging the initially selected line(s).
        QTest::mouseMove(plot,pixel({.75,.75}));QTest::mouseMove(plot,destination);
        QTest::mouseRelease(plot,Qt::LeftButton,Qt::NoModifier,destination);
        const double actualTimes[]{vm.cursor1(),vm.cursor2()},actualAmps[]{vm.amplitudeCursor1(),vm.amplitudeCursor2()};
        for(int i=0;i<2;++i) {
            if(timeCursor==i+1) QVERIFY(std::abs((actualTimes[i]-r.from)/(r.to-r.from)-.9)<.003);
            else QCOMPARE(actualTimes[i],times[i]);
            if(amplitudeCursor==i+1) QVERIFY(std::abs((actualAmps[i]-r.minimum)/(r.maximum-r.minimum)-.1)<.003);
            else QCOMPARE(actualAmps[i],amps[i]);
        }
        QCOMPARE(vm.deltaTime(),std::abs(vm.cursor2()-vm.cursor1()));
        QCOMPARE(vm.deltaAmplitude(),std::abs(static_cast<long double>(vm.amplitudeCursor2())-vm.amplitudeCursor1()));
        QCOMPARE(vm.viewRange().from,r.from);QCOMPARE(vm.viewRange().maximum,r.maximum);
    }
    void directCursorGuards()
    {
        WfmViewerViewModel vm;WaveformPlot plot(&vm);plot.resize(800,500);plot.show();
        vm.load(official("data_test_waveform.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(vm.hasTrace());
        plot.setInteractionMode(WaveformPlot::InteractionMode::Cursors);
        const auto full=vm.viewRange();const auto area=plot.plotArea();
        const auto pixel=[&](double x,double y){return QPoint(qRound(area.left()+x*area.width()),qRound(area.top()+y*area.height()));};
        const auto drag=[&](QPoint from,QPoint to){QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(&plot,to);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,to);};
        const double t1=vm.cursor1(),t2=vm.cursor2(),y1=vm.amplitudeCursor1(),y2=vm.amplitudeCursor2();
        drag(pixel(.25,.5)+QPoint(9,0),pixel(.4,.5));QCOMPARE(vm.cursor1(),t1);
        drag(pixel(.25,.5)+QPoint(7,0),pixel(.4,.5));QVERIFY(vm.cursor1()!=t1);
        vm.setCursor(1,t1);
        // Real double-click on an intersection restores the first click's two-axis movement.
        const auto near=pixel(.25,.75)+QPoint(3,3);
        QTest::mouseClick(&plot,Qt::LeftButton,Qt::NoModifier,near);QVERIFY(vm.cursor1()!=t1);QVERIFY(vm.amplitudeCursor1()!=y1);
        QTest::mouseDClick(&plot,Qt::LeftButton,Qt::NoModifier,near);QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,near);
        QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.amplitudeCursor1(),y1);
        // Coincident cursors select line 1 deterministically on each axis.
        vm.setCursor(2,t1);vm.setAmplitudeCursor(2,y1);
        drag(pixel(.25,.75),pixel(.6,.4));QCOMPARE(vm.cursor2(),t1);QCOMPARE(vm.amplitudeCursor2(),y1);
        QVERIFY(vm.cursor1()!=t1);QVERIFY(vm.amplitudeCursor1()!=y1);
        vm.setCursor(1,t1);vm.setCursor(2,t2);vm.setAmplitudeCursor(1,y1);vm.setAmplitudeCursor(2,y2);
        // A cursor just outside the visible edge must not be selected even within 8 px.
        const double span=full.to-full.from,height=full.maximum-full.minimum;
        vm.zoomTo({t1+span*.001,t2-span*.001,y1+height*.001,y2-height*.001});
        drag(pixel(0,1),pixel(.5,.5));QCOMPARE(vm.cursor1(),t1);QCOMPARE(vm.amplitudeCursor1(),y1);
        vm.fit();
        QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.25,.75));
        const double pressedT=vm.cursor1(),pressedY=vm.amplitudeCursor1();
        plot.setInteractionMode(WaveformPlot::InteractionMode::Pan);
        QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.9,.1));
        QCOMPARE(vm.cursor1(),pressedT);QCOMPARE(vm.amplitudeCursor1(),pressedY);QCOMPARE(vm.viewRange().from,full.from);
        plot.setInteractionMode(WaveformPlot::InteractionMode::Cursors);
        QTest::mousePress(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.25,.75));
        QTemporaryDir output;vm.exportCsv(output.filePath("busy.csv"));QVERIFY(vm.busy());
        QTest::mouseMove(&plot,pixel(.9,.1));QTest::mouseRelease(&plot,Qt::LeftButton,Qt::NoModifier,pixel(.9,.1));
        QCOMPARE(vm.cursor1(),pressedT);QCOMPARE(vm.amplitudeCursor1(),pressedY);
        QTRY_VERIFY(!vm.busy());
    }
    void basicViewerScope()
    {
        WfmViewerViewModel vm;
        WfmViewerPage page(&vm);
        QVERIFY(!page.findChild<QLineEdit *>("gain"));
        QVERIFY(!page.findChild<QPushButton *>("applyProcessing"));
        QVERIFY(!page.findChild<QPushButton *>("centerCursors"));
        QVERIFY(!page.findChild<QPushButton *>("zoomOut"));
        QVERIFY(page.findChild<QPushButton *>("exportCsv"));
    }
    void compactLayout()
    {
        QTemporaryDir temp;
        const auto folder=temp.filePath(QString(65,'d'));
        QVERIFY(QDir().mkpath(folder));
        const auto path=folder+"/"+QString(70,'w')+".wfm";
        QVERIFY(QFile::copy(official("data_test_waveform.wfm"),path));
        WfmViewerViewModel vm;WfmViewerPage page(&vm);page.resize(1480,780);page.show();
        auto *source=page.findChild<QLineEdit*>("waveformSource");
        auto *stats=page.findChild<QLabel*>("waveformStatistics");
        auto *plot=page.findChild<WaveformPlot*>();
        QVERIFY(source);QVERIFY(source->isReadOnly());QVERIFY(!stats->isVisible());
        vm.load(path);QTRY_VERIFY(!vm.busy());QVERIFY2(vm.hasTrace(),qPrintable(vm.error()));
        QCOMPARE(source->text(),path);QCOMPARE(source->toolTip(),path);
        source->selectAll();source->copy();QCOMPARE(QApplication::clipboard()->text(),path);source->deselect();
        const auto fullStats=stats->text();QVERIFY(fullStats.contains("Full waveform"));
        QVERIFY(fullStats.contains("Max:"));QVERIFY(fullStats.contains("Min:"));
        QVERIFY(!fullStats.contains("Mean:"));QVERIFY(!fullStats.contains("RMS:"));
        const auto full=vm.viewRange();
        const QList<QLabel*> labels{
            page.findChild<QLabel*>("cursor1Readout"),page.findChild<QLabel*>("cursor2Readout"),
            page.findChild<QLabel*>("deltaTimeReadout"),page.findChild<QLabel*>("amplitudeCursor1Readout"),
            page.findChild<QLabel*>("amplitudeCursor2Readout"),page.findChild<QLabel*>("deltaAmplitudeReadout")};
        for (bool outside : {false,true}) {
            if (outside) { vm.setCursor(1,vm.trace().start);vm.setAmplitudeCursor(1,full.minimum);vm.zoom(.1,0,0); }
            for (const auto size : {QSize(1480,780),QSize(900,640),QSize(760,540),QSize(1480,780)}) {
                page.resize(size);QCoreApplication::processEvents();
                QCOMPARE(page.size(),size);
                QVERIFY(plot->height()>size.height()*.65);
                for(const auto *name:{"cursorMode","panMode","zoomMode","fitWaveform","connectBounds"}) {
                    auto *control=page.findChild<QWidget*>(name);QVERIFY(control);QVERIFY(control->isVisible());
                    QVERIFY(page.rect().contains(control->geometry()));QVERIFY(control->width()>=control->sizeHint().width());
                }
                QVERIFY(source->geometry().right()<page.width());
                QCOMPARE(source->geometry().center().y(),page.findChild<QPushButton*>("openWfm")->geometry().center().y());
                for (int i=0;i<labels.size();++i) {
                    QVERIFY(labels[i]->isVisible());QVERIFY(page.rect().contains(labels[i]->geometry()));
                    QVERIFY2(labels[i]->width()>=labels[i]->sizeHint().width(),qPrintable(labels[i]->text()));
                    for(int j=i+1;j<labels.size();++j)QVERIFY(!labels[i]->geometry().intersects(labels[j]->geometry()));
                }
                QVERIFY(stats->geometry().bottom()<page.height());QCOMPARE(stats->text(),fullStats);
                QVERIFY(page.grab().save(TestEvidence::path(QStringLiteral("wfmviewer/layout-%1-%2.png").arg(size.width()).arg(outside))));
            }
        }
        vm.load(temp.filePath("missing.wfm"));QTRY_VERIFY(!vm.busy());QVERIFY(!vm.error().isEmpty());
        QCOMPARE(source->text(),path);QCOMPARE(stats->text(),fullStats);
        QVERIFY(page.findChild<QLabel*>("waveformStatus")->toolTip().contains("failed"));
    }
};
QTEST_MAIN(WfmViewerTest)
#include "wfm_viewer_test.moc"
