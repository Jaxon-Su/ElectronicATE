#include "../../src/service/waveformocr/waveformocrparser.h"
#include "../../src/infrastructure/storage/waveformocrfiles.h"
#include "../../src/infrastructure/storage/ziparchive.h"
#include "../../src/viewmodels/waveformocr/waveformocrviewmodel.h"
#include "../../src/views/waveformocr/waveformocrpage.h"
#include <QtTest>
#include <QDomDocument>
#include <QFontDatabase>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>

using namespace WaveformOcr;
namespace {
QString sample() {return R"({"channels":[{"channel":"CH1","scope":"current","max":"5.2 V","min":"-250 mV","rms":"3.5 V","mean":"2.4 V"},{"channel":"CH2","scope":"current","max":"120 mA","min":"-20 mA","rms":null,"mean":"50 mA"}]})";}
Cancellation cancellation() {return std::make_shared<std::atomic_bool>(false);}
bool makeImage(const QString &path) {QImage img(300,200,QImage::Format_RGB32);img.fill(Qt::white);return img.save(path);}
class HttpServer : public QObject {
public:
    QTcpServer server;
    int requests=0,delay=0,statusCode=200;
    QByteArray response=QJsonDocument(QJsonObject{{"done",true},{"response",sample()}}).toJson();
    QList<QByteArray> bodies;
    QList<QPointer<QTcpSocket>> sockets;
    HttpServer() {
        server.listen(QHostAddress::LocalHost,0);
        connect(&server,&QTcpServer::newConnection,this,[this] {
            while(server.hasPendingConnections()) {
                auto *socket=server.nextPendingConnection();sockets<<socket;
                auto bytes=std::make_shared<QByteArray>();
                connect(socket,&QTcpSocket::readyRead,this,[this,socket,bytes] {
                    *bytes+=socket->readAll();
                    if(socket->property("handled").toBool()) return;
                    const int end=bytes->indexOf("\r\n\r\n");if(end<0) return;
                    QRegularExpression re("Content-Length: ([0-9]+)",QRegularExpression::CaseInsensitiveOption);
                    const auto match=re.match(QString::fromLatin1(bytes->left(end)));
                    const int size=match.hasMatch()?match.captured(1).toInt():0;
                    if(bytes->size()<end+4+size) return;
                    socket->setProperty("handled",true);++requests;bodies<<bytes->mid(end+4,size);
                    const auto data=response;const int code=statusCode;
                    QTimer::singleShot(delay,socket,[socket,data,code] {
                        if(socket->state()!=QAbstractSocket::ConnectedState) return;
                        socket->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(data.size())+"\r\nConnection: close\r\n\r\n"+data);
                        socket->disconnectFromHost();
                    });
                });
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            }
        });
    }
    QUrl url() const {return QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));}
};
QList<QPair<QByteArray,QByteArray>> readParts(const QString &path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly)) return {};
    QList<QPair<QByteArray,QByteArray>> parts;ZipArchive::readStored(file.readAll(),parts);return parts;
}
QByteArray part(const QList<QPair<QByteArray,QByteArray>> &parts,const QByteArray &name) {
    for(const auto &p:parts) if(p.first==name) return p.second;return {};
}
}

class WaveformOcrTest : public QObject {
    Q_OBJECT
private slots:
    void units_data() {
        QTest::addColumn<QString>("text");QTest::addColumn<bool>("valid");QTest::addColumn<double>("number");QTest::addColumn<QString>("unit");
        QTest::newRow("milli")<<QString("-250 mV")<<true<<-0.25<<QString("V");
        QTest::newRow("micro")<<QString("−2.5 μA")<<true<<-2.5e-6<<QString("A");
        QTest::newRow("exponent")<<QString("1.2e-3 V")<<true<<0.0012<<QString("V");
        QTest::newRow("mega")<<QString("2 MOhm")<<true<<2e6<<QString("Ω");
        QTest::newRow("zero")<<QString("0 V")<<true<<0.0<<QString("V");
        for(const auto &s:QStringList{"NA","","5.2","1e999 V","1.2 foo","1 k%","NaN V","1,234 V","4V/2V"})
            QTest::newRow(qPrintable(s))<<s<<false<<0.0<<QString();
    }
    void units() {QFETCH(QString,text);QFETCH(bool,valid);QFETCH(double,number);QFETCH(QString,unit);const auto v=parseValue(text);QCOMPARE(v.number.has_value(),valid);if(valid){QVERIFY(std::abs(*v.number-number)<1e-12);QCOMPARE(v.unit,unit);}}
    void channels() {
        const auto rows=parseText("C:/data/load/a.png","C:/data",sample());QCOMPARE(rows.size(),2);
        QCOMPARE(rows[0].folder,QString("load"));QCOMPARE(rows[1].channel,QString("CH2"));
        QCOMPARE(*rows[0].values[1].number,-0.25);QVERIFY(!rows[1].values[2].number);
        QVERIFY(rows[0].rawResponse==sample());QVERIFY(!rows[0].reviewed);QVERIFY(issues(rows[1]).join(" ").contains("RMS"));
        auto weird=rows[0];weird.values[0]=parseValue("-1 V");QVERIFY(issues(weird).contains("Max 小於 Min"));
    }
    void textAmbiguity() {
        const auto rows=parseText("a.png",".","CH1\nMax: 5.2 V\nMin: -250 mV\nRMS: 3 V\nMean: 2 V\nCH2\nMaximum: 100 mA\nMinimum: -10 mA");
        QCOMPARE(rows.size(),2);QCOMPARE(*rows[0].values[1].number,-0.25);QCOMPARE(*rows[1].values[0].number,0.1);
        auto ambiguous=parseText("a.png",".","CH1\nRMS Mean Min Max\n3 V 2 V 1 V 5 V\nMax: 5 V\nMax: 6 V");
        QCOMPARE(ambiguous.size(),1);QVERIFY(!ambiguous[0].values[0].number);QVERIFY(!ambiguous[0].values[2].number);
        QCOMPARE(parseText("a.png",".","no measurements")[0].error,QString("未找到可辨識的量測欄位"));
    }
    void scan() {
        QTemporaryDir dir;QVERIFY(dir.isValid());QDir(dir.path()).mkdir("child");
        QVERIFY(makeImage(dir.filePath("10.PNG")));QVERIFY(makeImage(dir.filePath("2.png")));QVERIFY(makeImage(dir.filePath("child/1.png")));
        QFile junk(dir.filePath("skip.txt"));QVERIFY(junk.open(QIODevice::WriteOnly));junk.write("test");junk.close();
        auto cancel=cancellation();auto scanned=scanImages(dir.path(),cancel);QVERIFY(scanned.error.isEmpty());QCOMPARE(scanned.paths.size(),3);
        QVERIFY(scanned.paths.indexOf(dir.filePath("2.png"))<scanned.paths.indexOf(dir.filePath("10.PNG")));
        cancel->store(true);QVERIFY(!scanImages(dir.path(),cancel).error.isEmpty());QVERIFY(!scanImages(dir.filePath("missing"),cancellation()).error.isEmpty());
    }
    void tables() {
        const auto table=QString("| Measurement | Source | Current | Mean | Min | Max |\n|---|---|---|---|---|---|\n| RMS | CH1 | 3.5 V | 2 V | 1 V | 5 V |\n| Maximum | CH2 | 120 mA | 90 mA | 50 mA | 150 mA |");
        const auto wrapped=QString::fromUtf8(QJsonDocument(QJsonObject{{"text",table}}).toJson());
        auto rows=parseText("a.png",".",wrapped);QCOMPARE(rows.size(),2);QVERIFY(rows[0].values[2].number);QCOMPARE(*rows[0].values[2].number,3.5);
        QVERIFY(!rows[0].values[0].number);QVERIFY(!rows[0].values[1].number);QVERIFY(!rows[0].values[3].number);QCOMPARE(rows[0].scope,QString("current"));QCOMPARE(*rows[1].values[0].number,0.12);
        rows=parseText("a.png",".","<table><tr><th>Measurement</th><th>Source</th><th>Value</th></tr><tr><td>Maximum</td><td>C1</td><td>5 V</td></tr></table>");
        QCOMPARE(rows.size(),1);QCOMPARE(rows[0].channel,QString("CH1"));QCOMPARE(*rows[0].values[0].number,5.0);
        rows=parseText("a.png",".","|Measurement|Mean|Min|Max|\n|RMS|2 V|1 V|3 V|");QVERIFY(!rows[0].values[2].number);
        rows=parseText("a.png",".","|Measurement|Source|Current|\n|Mean of RMS|CH1|2 V|");QVERIFY(!rows[0].values[3].number);
        QVERIFY(!parseText("a.png",".","{\"unexpected\":true}")[0].error.isEmpty());
    }
    void excel() {
        QTemporaryDir dir;auto rows=parseText("C:/data/a.png","C:/data",sample());
        rows[0].folder=QString(40,'a')+"one";rows[1].folder=QString(40,'a')+"two";
        auto row=rows[0];row.folder="A/B";row.channel="=1+1";row.note="_x0041_ & <x>";rows<<row;
        row.folder="a?b";rows<<row;row.folder="History";rows<<row;
        const auto result=exportExcel(rows,dir.filePath("report.xlsx"),cancellation());QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QFile original(result.path);QVERIFY(original.open(QIODevice::ReadOnly));const auto before=original.readAll();original.close();
        const auto second=exportExcel(rows,dir.filePath("report.xlsx"),cancellation());QVERIFY(second.error.isEmpty());QVERIFY(result.path!=second.path);
        QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),before);
        const auto parts=readParts(result.path);QVERIFY(!parts.isEmpty());QDomDocument wb;QVERIFY(wb.setContent(part(parts,"xl/workbook.xml")));
        const auto sheets=wb.elementsByTagName("sheet");QCOMPARE(sheets.size(),5);QSet<QString> names;
        for(int i=0;i<sheets.size();++i) {const auto name=sheets.at(i).toElement().attribute("name");QVERIFY(name.size()<=31);QVERIFY(!names.contains(name.toCaseFolded()));names.insert(name.toCaseFolded());}
        bool hasNumber=false,hasLiteral=false,hasMissing=false;
        for(const auto &p:parts) {
            QDomDocument xml;QVERIFY2(xml.setContent(p.second),p.first.constData());
            if(!p.first.startsWith("xl/worksheets/")) continue;
            QVERIFY(xml.elementsByTagName("f").isEmpty());
            QCOMPARE(xml.elementsByTagName("row").at(0).toElement().elementsByTagName("c").size(),21);
            QVERIFY(p.second.contains("Manually edited"));
            QVERIFY(p.second.contains("Original OCR error"));
            QVERIFY(xml.elementsByTagName("autoFilter").at(0).toElement().attribute("ref").startsWith("A1:U"));
            hasNumber|=p.second.contains("<v>5.2000000000000002</v>");hasLiteral|=p.second.contains("=1+1");hasMissing|=p.second.contains("RMS: 缺值");
        }
        QVERIFY(hasNumber);QVERIFY(hasLiteral);QVERIFY(hasMissing);
        auto cancel=cancellation();cancel->store(true);QVERIFY(!exportExcel(rows,dir.filePath("cancel.xlsx"),cancel).error.isEmpty());QVERIFY(!QFileInfo::exists(dir.filePath("cancel.xlsx")));
        QVERIFY(!exportExcel(rows,dir.filePath("missing/report.xlsx"),cancellation()).error.isEmpty());
        const auto output=qEnvironmentVariable("OCR_REVIEW_DIR");if(!output.isEmpty()) QVERIFY(exportExcel(rows,QDir(output).filePath("independent-review.xlsx"),cancellation()).error.isEmpty());
    }
    void editAndReview() {
        OcrResultsModel model;model.append(parseText("a.png",".",sample()));const auto original=model.rows()[0].originalValues;
        for(int row=0;row<model.rowCount();++row) for(int col=0;col<OcrResultsModel::Count;++col) {
            const auto index=model.index(row,col);const auto before=model.data(index);
            const bool editable=(col>=OcrResultsModel::Max&&col<=OcrResultsModel::Mean)||col==OcrResultsModel::Channel||col==OcrResultsModel::Scope;
            QCOMPARE(bool(model.flags(index)&Qt::ItemIsEditable),editable);
            if(!editable) {QVERIFY(!model.setData(index,"6 mV",Qt::EditRole));QCOMPARE(model.data(index),before);}
            QCOMPARE(bool(model.flags(index)&Qt::ItemIsUserCheckable),col==OcrResultsModel::Review);
        }
        const auto maximum=model.index(0,OcrResultsModel::Max);
        QVERIFY(model.setData(maximum,"6 mV"));QCOMPARE(*model.rows()[0].values[0].number,0.006);
        QCOMPARE(model.rows()[0].originalValues,original);
        QVERIFY(model.setData(maximum,"5.2 V"));
        QVERIFY(model.setData(model.index(0,OcrResultsModel::Review),Qt::Checked,Qt::CheckStateRole));QVERIFY(model.rows()[0].reviewed);
        QCOMPARE(*model.rows()[0].values[0].number,5.2);QCOMPARE(model.rows()[0].originalValues,original);
        QVERIFY(model.setData(model.index(1,OcrResultsModel::Review),Qt::Checked,Qt::CheckStateRole));
        QVERIFY(!model.rows()[1].values[2].number);QVERIFY(issues(model.rows()[1]).join(" ").contains("RMS"));
        model.setLocked(true);QVERIFY(!model.setData(model.index(0,OcrResultsModel::Review),Qt::Unchecked,Qt::CheckStateRole));
        QVERIFY(!model.setData(maximum,"9 V"));QCOMPARE(*model.rows()[0].values[0].number,5.2);
        QVERIFY(!(model.flags(model.index(0,OcrResultsModel::Review))&Qt::ItemIsUserCheckable));QVERIFY(model.rows()[0].reviewed);
        model.setLocked(false);QVERIFY(model.setData(model.index(0,OcrResultsModel::Review),Qt::Unchecked,Qt::CheckStateRole));QVERIFY(!model.rows()[0].reviewed);
        model.append(parseText("failed.png",".","no measurements"));
        QVERIFY(model.setData(model.index(2,OcrResultsModel::Review),Qt::Checked,Qt::CheckStateRole));
        QVERIFY(!model.rows()[2].error.isEmpty());QCOMPARE(status(model.rows()[2]),QString("辨識失敗"));
    }
    void client() {
        QTemporaryDir dir;const auto path=dir.filePath("a.png");QVERIFY(makeImage(path));HttpServer server;OllamaClient client;
        QSignalSpy spy(&client,&OllamaClient::recognitionReady);client.recognize(server.url(),"  glm-ocr:latest  ",path);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,4000);QVERIFY(spy.last()[1].toString().isEmpty());QCOMPARE(spy.last()[0].toString(),sample());
        const auto body=QJsonDocument::fromJson(server.bodies[0]).object();QCOMPARE(body["prompt"].toString(),QString("Text Recognition:"));QVERIFY(!body["stream"].toBool());
        QVERIFY(!body["images"].toArray()[0].toString().isEmpty());
        QCOMPARE(body["format"].toString(),QString("json"));
        QCOMPARE(body["model"].toString(),QString("glm-ocr:latest"));
        QSignalSpy models(&client,&OllamaClient::modelsReady);server.response=R"({"models":[{"name":"glm-ocr:latest"}]})";
        client.listModels(server.url());QTRY_COMPARE_WITH_TIMEOUT(models.size(),1,4000);QCOMPARE(models[0][0].toStringList(),QStringList{"glm-ocr:latest"});
    }
    void clientErrors_data() {
        QTest::addColumn<QByteArray>("reply");QTest::addColumn<int>("status");QTest::addColumn<int>("delay");
        QTest::newRow("invalid-json")<<QByteArray("bad")<<200<<0;
        QTest::newRow("truncated")<<QByteArray(R"({"done":true,"done_reason":"length","response":"Max 5 V"})")<<200<<0;
        QTest::newRow("incomplete")<<QByteArray(R"({"done":false,"response":"Max 5 V"})")<<200<<0;
        QTest::newRow("empty")<<QByteArray(R"({"done":true,"response":""})")<<200<<0;
        QTest::newRow("model-error")<<QByteArray(R"({"error":"model not found"})")<<404<<0;
        QTest::newRow("redirect")<<QByteArray("{}")<<302<<0;
        QTest::newRow("oversize")<<QByteArray(1100000,'x')<<200<<0;
        QTest::newRow("timeout")<<QByteArray("{}")<<200<<200;
    }
    void clientErrors() {
        QFETCH(QByteArray,reply);QFETCH(int,status);QFETCH(int,delay);QTemporaryDir dir;const auto path=dir.filePath("a.png");QVERIFY(makeImage(path));
        HttpServer server;server.response=reply;server.statusCode=status;server.delay=delay;OllamaClient client;client.setTimeoutMs(100);
        QSignalSpy spy(&client,&OllamaClient::recognitionReady);client.recognize(server.url(),"vision",path);QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,4000);QVERIFY(!spy[0][1].toString().isEmpty());QVERIFY(!client.busy());
    }
    void cancel() {
        QTemporaryDir dir;const auto path=dir.filePath("a.png");QVERIFY(makeImage(path));HttpServer server;server.delay=200;OllamaClient client;
        QSignalSpy spy(&client,&OllamaClient::recognitionReady);client.recognize(server.url(),"vision",path);QTRY_COMPARE(server.requests,1);client.cancel();QVERIFY(!client.busy());
        server.delay=0;client.recognize(server.url(),"vision",path);QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,4000);QTest::qWait(250);QCOMPARE(spy.size(),1);
        QVERIFY(!OllamaClient::validateEndpoint(QUrl("http://192.168.0.6:11434")).isEmpty());
        QVERIFY(!OllamaClient::validateEndpoint(QUrl("http://localhost:11434@remote.test")).isEmpty());
        QVERIFY(OllamaClient::validateEndpoint(QUrl("http://[::1]:11434")).isEmpty());
    }
    void batch() {
        QTemporaryDir dir;for(int i=0;i<3;++i) QVERIFY(makeImage(dir.filePath(QString::number(i)+".png")));
        HttpServer server;WaveformOcrViewModel vm;vm.start(dir.path(),server.url(),"vision");QVERIFY(vm.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),8000);QCOMPARE(server.requests,3);QCOMPARE(vm.completed(),3);QCOMPARE(vm.results()->rowCount(),6);
        QVERIFY(vm.hasUnexportedResults());QSignalSpy exported(&vm,&WaveformOcrViewModel::exportFinished);vm.exportResults(dir.filePath("result.xlsx"));
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),4000);QCOMPARE(exported.size(),1);QVERIFY(!vm.hasUnexportedResults());
        QVERIFY(vm.results()->setData(vm.results()->index(0,OcrResultsModel::Review),Qt::Checked,Qt::CheckStateRole));QVERIFY(vm.hasUnexportedResults());
        vm.exportResults(dir.filePath("result.xlsx"));QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),4000);QCOMPARE(exported.size(),2);
        QVERIFY(!vm.hasUnexportedResults());QVERIFY(exported[0][0].toString()!=exported[1][0].toString());
        QDomDocument reviewed;QVERIFY(reviewed.setContent(part(readParts(exported[1][0].toString()),"xl/worksheets/sheet1.xml")));
        bool reviewSaved=false;const auto reviewCells=reviewed.elementsByTagName("c");
        for(int i=0;i<reviewCells.size();++i) {const auto c=reviewCells.at(i).toElement();if(c.attribute("r")=="O2") reviewSaved=c.text()=="Yes";}
        QVERIFY(reviewSaved);
        server.delay=300;vm.start(dir.path(),server.url(),"vision");QTRY_COMPARE(server.requests,4);vm.stop();QVERIFY(!vm.busy());QTest::qWait(350);QCOMPARE(vm.completed(),0);QCOMPARE(vm.results()->rowCount(),0);
        server.delay=0;server.response=R"({"error":"temporary failure"})";vm.start(dir.path(),server.url(),"vision");QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),8000);
        QCOMPARE(vm.results()->rowCount(),3);QVERIFY(vm.message().contains("3 張無有效數值"));
        for(const auto &row:vm.results()->rows()) {QVERIFY(!row.error.isEmpty());for(const auto &value:row.values) QVERIFY(!value.number);}
        HttpServer partialServer;WaveformOcrViewModel partial;
        connect(&partial,&WaveformOcrViewModel::imageStarted,&partial,[&](const QString &path) {if(QFileInfo(path).baseName()=="1") partialServer.delay=250;});
        partial.start(dir.path(),partialServer.url(),"vision");QTRY_COMPARE(partialServer.requests,2);partial.stop();
        QCOMPARE(partial.completed(),1);QCOMPARE(partial.results()->rowCount(),2);QTest::qWait(300);QCOMPARE(partial.results()->rowCount(),2);
        partial.start(dir.filePath("missing"),partialServer.url(),"vision");QVERIFY(!partial.busy());QCOMPARE(partial.results()->rowCount(),2);
        partial.exportResults(dir.filePath("missing/result.xlsx"));QTRY_VERIFY(!partial.busy());QVERIFY(partial.hasUnexportedResults());QVERIFY(partial.message().contains("匯出未完成"));
    }
    void view() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");QApplication::setFont(QFont("Microsoft JhengHei",10));
        QTemporaryDir dir;const auto path=dir.filePath("a.png");QVERIFY(makeImage(path));WaveformOcrViewModel vm;WaveformOcrPage page(&vm);page.resize(1200,760);page.show();
        const auto fixture=qEnvironmentVariable("OCR_FIXTURE");vm.results()->append(parseText(fixture.isEmpty()?path:fixture,dir.path(),sample()));
        auto *table=page.findChild<QTableView*>("ocrResults");QVERIFY(table);table->selectRow(0);QTest::qWait(50);
        auto *preview=page.findChild<QGraphicsView*>("ocrPreview");QVERIFY(preview);QVERIFY(!preview->scene()->items().isEmpty());
        const auto selectedImageSize=preview->scene()->sceneRect().size();
        const auto nextPath=dir.filePath("next.png");QImage nextImage(73,41,QImage::Format_RGB32);nextImage.fill(Qt::red);QVERIFY(nextImage.save(nextPath));
        vm.imageStarted(nextPath);
        QCOMPARE(preview->scene()->sceneRect().size(),selectedImageSize);
        QVERIFY(page.findChild<QPlainTextEdit*>("ocrRawResponse")->toPlainText()==sample());
        QCOMPARE(table->editTriggers(),QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
        QVERIFY(page.findChild<QPlainTextEdit*>("ocrDetails")->toPlainText().contains("5.2 V"));
        QVERIFY(vm.results()->setData(vm.results()->index(0,OcrResultsModel::Max),"10 mV"));QCOMPARE(*vm.results()->rows()[0].values[0].number,0.01);
        QCOMPARE(page.findChild<QPlainTextEdit*>("ocrRawResponse")->toPlainText(),sample());
        QVERIFY(!page.findChild<QLineEdit*>("ocrEndpoint"));
        for(const auto *button:page.findChildren<QPushButton*>()) QVERIFY(!button->text().contains("新增通道"));
        table->setCurrentIndex(vm.results()->index(0,OcrResultsModel::Review));table->setFocus();
        QTest::keyClick(table,Qt::Key_Space);QVERIFY(vm.results()->rows()[0].reviewed);
        QTest::keyClick(table,Qt::Key_Space);QVERIFY(!vm.results()->rows()[0].reviewed);
        QTest::keyClick(table,Qt::Key_Space);QVERIFY(vm.results()->rows()[0].reviewed);
        QVERIFY(page.findChild<QPushButton*>("ocrExport")->isEnabled());
        const auto output=qEnvironmentVariable("OCR_REVIEW_DIR");if(!output.isEmpty()) QVERIFY(page.grab().save(QDir(output).filePath("waveformOcr-wide.png")));
        page.resize(800,600);QTest::qWait(50);QVERIFY(page.width()<=800);QVERIFY(page.height()<=600);
        const auto *button=page.findChild<QPushButton*>("ocrExport");QVERIFY(page.rect().contains(QRect(button->mapTo(&page,QPoint()),button->size())));
        if(!output.isEmpty()) QVERIFY(page.grab().save(QDir(output).filePath("waveformOcr-narrow.png")));
    }
    void liveOllama() {
        const auto fixture=qEnvironmentVariable("OCR_FIXTURE");if(fixture.isEmpty()) QSKIP("Explicit local Ollama fixture required");
        OllamaClient client;QSignalSpy spy(&client,&OllamaClient::recognitionReady);
        for(int i=0;i<2;++i) {
            client.recognize(QUrl("http://127.0.0.1:11434"),"glm-ocr:latest",fixture);QTRY_COMPARE_WITH_TIMEOUT(spy.size(),i+1,120000);
            const auto text=spy[i][0].toString();const auto error=spy[i][1].toString();
            const auto output=qEnvironmentVariable("OCR_REVIEW_DIR");QFile log(QDir(output).filePath(QString("live-%1.txt").arg(i)));QVERIFY(log.open(QIODevice::WriteOnly));log.write((error+"\n"+text).toUtf8());log.close();
            QVERIFY2(error.isEmpty(),qPrintable(error));const auto rows=parseText(fixture,QFileInfo(fixture).absolutePath(),text);
            QCOMPARE(rows.size(),2);QVERIFY(rows[0].values[0].number);QCOMPARE(*rows[0].values[0].number,5.2);QCOMPARE(*rows[0].values[1].number,-0.25);QCOMPARE(*rows[1].values[0].number,0.12);
        }
    }
    void liveBatch() {
        const auto fixture=qEnvironmentVariable("OCR_FIXTURE");if(fixture.isEmpty()) QSKIP("Explicit local Ollama fixture required");
        QTemporaryDir dir;QVERIFY(QDir(dir.path()).mkpath("A/B"));QVERIFY(QDir(dir.path()).mkpath("A_B"));
        QVERIFY(QFile::copy(fixture,dir.filePath("A/B/capture.png")));QVERIFY(QFile::copy(fixture,dir.filePath("A_B/capture.png")));
        WaveformOcrViewModel vm;vm.start(dir.path(),QUrl("http://127.0.0.1:11434"),"glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),120000);QCOMPARE(vm.completed(),2);QCOMPARE(vm.results()->rowCount(),4);
        const std::array<double,4> channel1{5.2,-0.25,3.5,2.4},channel2{0.12,-0.02,0.08,0.05};
        for(const auto &row:vm.results()->rows()) {
            QVERIFY(row.error.isEmpty());QVERIFY(row.channel=="CH1"||row.channel=="CH2");
            const auto expected=row.channel=="CH1"?channel1:channel2;
            for(int i=0;i<4;++i) {QVERIFY(row.values[i].number);QVERIFY(std::abs(*row.values[i].number-expected[i])<1e-12);}
        }
        QSignalSpy exported(&vm,&WaveformOcrViewModel::exportFinished);
        vm.exportResults(QDir(qEnvironmentVariable("OCR_REVIEW_DIR")).filePath("live-batch.xlsx"));
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(),4000);QCOMPARE(exported.size(),1);
        QDomDocument workbook;QVERIFY(workbook.setContent(part(readParts(exported[0][0].toString()),"xl/workbook.xml")));
        QCOMPARE(workbook.elementsByTagName("sheet").size(),2);
    }
};
QTEST_MAIN(WaveformOcrTest)
#include "waveform_ocr_test.moc"
