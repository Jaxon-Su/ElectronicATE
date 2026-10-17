#include "service/waveformocr/waveformocrparser.h"
#include "viewmodels/waveformocr/waveformocrviewmodel.h"
#include "views/waveformocr/waveformocrpage.h"
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtEndian>

using namespace WaveformOcr;
static const QString completeText = "Max 5 V\nMin -2 V\nRMS 3 V\nMean 1 V";

// A real local HTTP connection tests cancellation, request serialization and delivery.
class OllamaStub : public QTcpServer
{
  public:
    QStringList responses{completeText};
    int requests = 0, delayMs = 0;
    QList<QByteArray> bodies;
    OllamaStub()
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                auto input = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, input] {
                    input->append(socket->readAll());
                    if (socket->property("handled").toBool()) return;
                    const auto end = input->indexOf("\r\n\r\n");
                    if (end < 0) return;
                    int length = 0;
                    for (const auto &line : input->left(end).split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toInt();
                    if (input->size() < end + 4 + length) return;
                    socket->setProperty("handled", true);
                    bodies << input->mid(end + 4, length);
                    const int request = requests++;
                    const bool models = input->startsWith("GET /api/tags ");
                    QByteArray json = models ? QByteArray(R"({"models":[{"name":"glm-ocr:latest"}]})") :
                        QJsonDocument(QJsonObject{{"response", responses[request % responses.size()]},
                                                  {"done", true}, {"done_reason", "stop"}}).toJson();
                    QTimer::singleShot(delayMs, socket, [socket, json] {
                        if (socket->state() != QAbstractSocket::ConnectedState) return;
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                      QByteArray::number(json.size()) + "\r\nConnection: close\r\n\r\n" + json);
                        socket->disconnectFromHost();
                    });
                });
            }
        });
    }
    QUrl endpoint() const { return QUrl("http://127.0.0.1:" + QString::number(serverPort())); }
};

static bool makeImage(const QString &path)
{
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::black);
    return image.save(path);
}

// The production writer uses uncompressed ZIP members. Read by local headers,
// independently of the writer, to validate the actual worksheet content.
static QByteArray zipMember(const QString &path, const QByteArray &member)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.readAll();
    qsizetype pos = 0;
    while (pos + 30 <= bytes.size() && bytes.mid(pos, 4) == QByteArray("PK\x03\x04", 4)) {
        auto u16 = [&](int offset) { return qFromLittleEndian<quint16>(bytes.constData() + pos + offset); };
        auto u32 = [&](int offset) { return qFromLittleEndian<quint32>(bytes.constData() + pos + offset); };
        const auto size = u32(18);
        const auto nameSize = u16(26), extraSize = u16(28);
        const auto start = pos + 30 + nameSize + extraSize;
        if (start + size > bytes.size() || u16(8) != 0) return {};
        if (bytes.mid(pos + 30, nameSize) == member) return bytes.mid(start, size);
        pos = start + size;
    }
    return {};
}

class WaveformOcrRegression : public QObject
{
    Q_OBJECT
  private slots:
    void editsAndOriginalEvidence()
    {
        WaveformOcrViewModel vm;
        auto *model = vm.results();
        const auto rows = parseText("samples/1.png", "samples", completeText);
        model->append(rows);
        QSignalSpy edited(model, &OcrResultsModel::resultEdited);
        const auto max = model->index(0, OcrResultsModel::Max);
        const auto review = model->index(0, OcrResultsModel::Review);
        QVERIFY(model->setData(review, Qt::Checked, Qt::CheckStateRole));
        edited.clear();
        QVERIFY(model->setData(max, "5000 mV"));
        QVERIFY(model->setData(review, Qt::Checked, Qt::CheckStateRole));
        QVERIFY(model->setData(model->index(0, OcrResultsModel::Scope), ""));
        QCOMPARE(edited.count(), 0);
        QVERIFY(model->row(0)->reviewed && !model->row(0)->modified);
        QVERIFY(model->setData(max, "6"));
        QCOMPARE(*model->row(0)->values[0].number, 6.0);
        QCOMPARE(model->row(0)->values[0].unit, "V");
        QVERIFY(model->row(0)->modified && !model->row(0)->reviewed);
        QCOMPARE(model->row(0)->originalValues, rows[0].originalValues);
        QCOMPARE(model->row(0)->rawResponse, completeText);
        QCOMPARE(vm.selection(0).imagePath, rows[0].imagePath);
        QVERIFY(vm.selection(0).details.contains("已修改"));
        QVERIFY(vm.selection(-1).imagePath.isEmpty());
        OcrResultsModel other;
        other.append(rows);
        QVERIFY(!model->setData(other.index(0, OcrResultsModel::Max), "9 V"));
        QVERIFY(!model->setData(review, Qt::PartiallyChecked, Qt::CheckStateRole));
        model->setLocked(true);
        QVERIFY(!(model->flags(max) & Qt::ItemIsEditable));
        QVERIFY(!model->setData(max, "9 V"));
        model->setLocked(false);
        QVERIFY(model->setData(max, "bad value"));
        QVERIFY(!model->row(0)->values[0].number);
        QVERIFY(!issues(*model->row(0)).isEmpty());
        auto failed = failedRow("samples/2.png", "samples", "original error");
        model->append({failed});
        QVERIFY(model->setData(model->index(1, OcrResultsModel::Max), "2"));
        QVERIFY(!model->row(1)->values[0].number);
        QVERIFY(model->setData(model->index(1, OcrResultsModel::Max), "2 A"));
        QVERIFY(model->setData(model->index(1, OcrResultsModel::Min), "-1"));
        QCOMPARE(model->row(1)->values[1].unit, "A");
        QCOMPARE(model->row(1)->error, "original error");
        QVERIFY(!status(*model->row(1)).contains("辨識失敗"));
        QVERIFY(model->setData(model->index(1, OcrResultsModel::Max), ""));
        QVERIFY(!model->row(1)->values[0].number);
    }
    void editorEnterEscapeAndMetadata()
    {
        WaveformOcrViewModel vm;
        WaveformOcrPage view(&vm);
        view.resize(1400, 850);
        view.show();
        vm.results()->append(parseText("samples/1.png", "samples", completeText));
        auto *table = view.findChild<QTableView*>("ocrResults");
        auto *button = view.findChild<QPushButton*>("ocrEdit");
        auto *metadata = view.findChild<QCheckBox*>("ocrMetadata");
        QVERIFY(table && button && metadata);
        QVERIFY(table->isColumnHidden(OcrResultsModel::Channel));
        metadata->setChecked(true);
        QVERIFY(!table->isColumnHidden(OcrResultsModel::Scope));
        table->setCurrentIndex(vm.results()->index(0, OcrResultsModel::Max));
        QTest::mouseClick(button, Qt::LeftButton);
        auto *editor = table->findChild<QLineEdit*>();
        QVERIFY(editor);
        editor->selectAll();
        QTest::keyClicks(editor, "7 V");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(*vm.results()->row(0)->values[0].number, 7.0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::keyClick(table, Qt::Key_F2);
        editor = table->findChild<QLineEdit*>();
        QVERIFY(editor);
        editor->selectAll();
        QTest::keyClicks(editor, "99 V");
        QTest::keyClick(editor, Qt::Key_Escape);
        QCOMPARE(*vm.results()->row(0)->values[0].number, 7.0);
    }
    void batchCountsAndExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 1; i <= 3; ++i) QVERIFY(makeImage(dir.filePath(QString::number(i) + ".png")));
        OllamaStub server;
        QVERIFY(server.isListening());
        server.responses = {completeText, "Max 4 V", "unreadable"};
        WaveformOcrViewModel vm;
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(server.requests, 3);
        QCOMPARE(vm.completed(), 3);
        QCOMPARE(vm.total(), 3);
        QCOMPARE(vm.results()->rowCount(), 3);
        QVERIFY(vm.message().contains("1 張四值齊全、1 張部分缺值、1 張無有效數值"));
        QVERIFY(vm.hasUnexportedResults());
        const auto request = QJsonDocument::fromJson(server.bodies.first()).object();
        QCOMPARE(request.value("prompt").toString(), "Text Recognition:");
        QCOMPARE(request.value("model").toString(), "glm-ocr:latest");
        const auto original = vm.results()->row(0)->originalValues;
        vm.results()->setData(vm.results()->index(0, OcrResultsModel::Max), "6 V");
        QSignalSpy exported(&vm, &WaveformOcrViewModel::exportFinished);
        const auto path = dir.filePath("report.xlsx");
        vm.exportResults(path);
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(exported.count(), 1);
        QVERIFY(!vm.hasUnexportedResults());
        const auto xml = zipMember(path, "xl/worksheets/sheet1.xml");
        QVERIFY(xml.contains("<c r=\"E2\"><v>6</v></c>"));
        QVERIFY(xml.contains(original.toUtf8()));
        QVERIFY(xml.contains("Manually edited") && xml.contains("Original OCR error"));
        QVERIFY(xml.contains("<c r=\"T2\" t=\"inlineStr\"><is><t xml:space=\"preserve\">Yes</t>"));
        QVERIFY(xml.contains("A1:U4"));
        vm.exportResults(path);
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(exported.count(), 2);
        QVERIFY(QFile::exists(dir.filePath("report_2.xlsx")));
        QCOMPARE(zipMember(path, "xl/worksheets/sheet1.xml"), xml);
        vm.refreshModels(server.endpoint());
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QVERIFY(!vm.hasUnexportedResults()); // Lock-state notifications are not edits.
        vm.results()->setData(vm.results()->index(0, OcrResultsModel::Max), "6 V");
        QVERIFY(!vm.hasUnexportedResults());
        vm.results()->setData(vm.results()->index(0, OcrResultsModel::Max), "7 V");
        QVERIFY(vm.hasUnexportedResults());
        const auto cancel = std::make_shared<std::atomic_bool>(true);
        const auto cancelled = exportExcel(vm.results()->rows(), dir.filePath("cancel.xlsx"), cancel);
        QVERIFY(!cancelled.error.isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("cancel.xlsx")));
        vm.exportResults(dir.filePath("missing/report.xlsx"));
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QVERIFY(vm.hasUnexportedResults());
    }
    void stopBeforeRequestAndRestart()
    {
        QTemporaryDir dir;
        QVERIFY(makeImage(dir.filePath("1.png")));
        OllamaStub server;
        WaveformOcrViewModel vm;
        auto stop = connect(&vm, &WaveformOcrViewModel::imageStarted, &vm, &WaveformOcrViewModel::stop);
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QTest::qWait(50);
        QCOMPARE(server.requests, 0);
        QCOMPARE(vm.results()->rowCount(), 0);
        disconnect(stop);
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(server.requests, 1);
        QCOMPARE(vm.results()->rowCount(), 1);
    }
    void stopPendingReply()
    {
        QTemporaryDir dir;
        QVERIFY(makeImage(dir.filePath("1.png")));
        OllamaStub server;
        server.delayMs = 150;
        WaveformOcrViewModel vm;
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_COMPARE_WITH_TIMEOUT(server.requests, 1, 5000);
        vm.stop();
        QVERIFY(!vm.busy());
        server.delayMs = 0;
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QTest::qWait(200);
        QCOMPARE(server.requests, 2);
        QCOMPARE(vm.completed(), 1);
        QCOMPARE(vm.results()->rowCount(), 1);
    }
    void stopOnRowInsertion()
    {
        QTemporaryDir dir;
        QVERIFY(makeImage(dir.filePath("1.png")));
        QVERIFY(makeImage(dir.filePath("2.png")));
        OllamaStub server;
        WaveformOcrViewModel vm;
        connect(vm.results(), &QAbstractItemModel::rowsInserted, &vm, &WaveformOcrViewModel::stop);
        vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QTest::qWait(50);
        QCOMPARE(server.requests, 1);
        QCOMPARE(vm.completed(), 1);
        QVERIFY(vm.message().contains("保留 1 張"));
    }
    void cancelScanKeepsResultLocation()
    {
        QTemporaryDir original, other;
        QVERIFY(makeImage(original.filePath("1.png")));
        QVERIFY(makeImage(other.filePath("2.png")));
        OllamaStub server;
        WaveformOcrViewModel vm;
        vm.start(original.path(), server.endpoint(), "glm-ocr:latest");
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        const auto source = vm.selection(0).imagePath;
        vm.start(other.path(), server.endpoint(), "glm-ocr:latest");
        vm.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(vm.selection(0).imagePath, source);
        QCOMPARE(QFileInfo(vm.suggestedExportPath()).absolutePath(), original.path());
        QCOMPARE(server.requests, 1);
    }
    void modelSignalsCanCancelOrStartBatch()
    {
        OllamaStub server;
        WaveformOcrViewModel vm;
        auto stop = connect(&vm, &WaveformOcrViewModel::changed, &vm, [&] {
            if (vm.busy()) vm.stop();
        });
        vm.refreshModels(server.endpoint());
        QTest::qWait(50);
        QVERIFY(!vm.busy());
        QCOMPARE(server.requests, 0);
        disconnect(stop);
        QTemporaryDir dir;
        QVERIFY(makeImage(dir.filePath("1.png")));
        connect(&vm, &WaveformOcrViewModel::modelNamesChanged, &vm, [&](const QStringList &names) {
            QCOMPARE(names, QStringList{"glm-ocr:latest"});
            vm.stop();
            vm.start(dir.path(), server.endpoint(), "glm-ocr:latest");
        });
        vm.refreshModels(server.endpoint());
        QTRY_VERIFY_WITH_TIMEOUT(!vm.busy(), 5000);
        QCOMPARE(server.requests, 2);
        QCOMPARE(vm.completed(), 1);
    }
    void remoteEndpointRejected()
    {
        QVERIFY(!OllamaClient::validateEndpoint(QUrl("https://example.com")).isEmpty());
        QVERIFY(!OllamaClient::validateEndpoint(QUrl("http://127.0.0.1/?redirect=x")).isEmpty());
        QVERIFY(OllamaClient::validateEndpoint(QUrl("http://[::1]:11434")).isEmpty());
    }
};
QTEST_MAIN(WaveformOcrRegression)
#include "waveform_ocr_regression.moc"
