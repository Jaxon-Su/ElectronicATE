#include "xmlconfigstore.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class TestPage : public QObject, public IXmlSerializable {
public:
    QString value;
    QString tag = "Page1";
    bool failApply = false;
    int publications = 0;
    std::function<void()> onPublish;
    void publishXmlLoaded() override { ++publications; if (onPublish) onPublish(); }
    int loads = 0;
    bool failWrite = false;
    QString xmlTagName() const override { return tag; }
    void writeXml(QXmlStreamWriter& writer) const override
    {
        writer.writeTextElement(xmlTagName(), failWrite ? QString(QChar(1)) : value);
    }
    void validateXml(QXmlStreamReader& reader) const override { reader.readElementText(); }
    void loadXml(QXmlStreamReader& reader) override
    {
        ++loads;
        value = reader.readElementText();
        if (failApply && value == "reject") reader.raiseError("apply failed");
    }
};

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "fixture open failed");
    require(file.write(bytes) == bytes.size(), "fixture write failed");
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "fixture read failed");
    return file.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        const QString path = dir.filePath("config.xml");
        TestPage source;
        source.value = QString::fromUtf8("測試 & <value>");
        require(XmlConfigStore::saveAllToXml(path, {nullptr, &source}).succeeded(), "save failed");
        TestPage target;
        require(XmlConfigStore::loadAllFromXml(path, {nullptr, &target}).succeeded(), "load failed");
        require(target.value == source.value && target.loads == 1, "round trip mismatch");

        const QByteArray original = readFile(path);
        source.failWrite = true;
        require(XmlConfigStore::saveAllToXml(path, {&source}).error == XmlOperationResult::Error::Write,
                "invalid XML character should fail serialization");
        require(readFile(path) == original, "failed save overwrote existing file");
        require(XmlConfigStore::saveAllToXml(dir.filePath("missing/config.xml"), {&target}).error
                    == XmlOperationResult::Error::Open, "missing directory should fail");
        require(XmlConfigStore::loadAllFromXml(dir.filePath("missing.xml"), {&target}).error
                    == XmlOperationResult::Error::Open, "missing file should fail");

        const QByteArray malformed[] = {
            "", "<loodGUI><Page1>new</Page1><broken></loodGUI>",
            "<loodGUI><Page1>new</Page1></loodGUI><extra/>"
        };
        for (const auto& document : malformed) {
            writeFile(path, document);
            const auto result = XmlConfigStore::loadAllFromXml(path, {&target});
            require(result.error == XmlOperationResult::Error::Parse, "malformed XML accepted");
            require(target.loads == 1 && target.value == source.value,
                    "malformed XML mutated page before validation");
        }
        writeFile(path, "<other><loodGUI><Page1>new</Page1></loodGUI></other>");
        require(XmlConfigStore::loadAllFromXml(path, {&target}).error == XmlOperationResult::Error::InvalidRoot,
                "nested expected root should be rejected");
        require(target.loads == 1, "wrong root mutated page");

        writeFile(path, "<loodGUI><Future><Page1>hidden</Page1></Future><Page1>visible</Page1></loodGUI>");
        require(XmlConfigStore::loadAllFromXml(path, {&target}).succeeded(), "unknown section rejected");
        require(target.loads == 2 && target.value == "visible", "nested unknown section dispatched");
        writeFile(path, "<loodGUI/>");
        require(XmlConfigStore::loadAllFromXml(path, {&target}).succeeded(), "empty root rejected");
        require(target.loads == 2, "empty root mutated page");
        TestPage first, second;
        second.tag = "Page2";
        first.value = "old-first";
        second.value = "old-second";
        second.failApply = true;
        writeFile(path, "<loodGUI><Page1>new-first</Page1><Page2>reject</Page2></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded(), "late apply failure accepted");
        require(first.value == "old-first" && second.value == "old-second", "late failure left a mixed configuration");
        require(first.publications == 0 && second.publications == 0, "failed transaction published changes");
        writeFile(path, "<loodGUI><Page2>new-second</Page2><Page1>new-first</Page1></loodGUI>");
        bool completeAtNotification = false, nestedRejected = false;
        first.onPublish = [&] {
            completeAtNotification = first.value == "new-first" && second.value == "new-second";
            nestedRejected = !XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded();
        };
        bool synchronized = false;
        require(XmlConfigStore::loadAllFromXml(path, {&first, &second}, [&] {
            synchronized = first.signalsBlocked() && second.signalsBlocked();
        }).succeeded(), "transaction failed");
        require(completeAtNotification && nestedRejected && synchronized, "transaction exposed incomplete state or reentrant load");
        writeFile(path, "<loodGUI><Page1>one</Page1><Page1>two</Page1></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded(), "duplicate sections accepted");
        require(first.value == "new-first", "duplicate section changed current data");
        auto* closing = new TestPage;
        closing->tag = "Page2";
        first.onPublish = [&] { delete closing; closing = nullptr; };
        writeFile(path, "<loodGUI><Page1>last</Page1><Page2>last</Page2></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, closing}).succeeded() && !closing,
                "notification dereferenced a closed page");
        std::cout << "PASS: round trip, atomic failure, open errors, preflight and section dispatch\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
