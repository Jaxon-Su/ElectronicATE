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
    QString tag = "InstrumentSetup";
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

class CheckpointPage : public TestPage {
public:
    bool failRollback = false;
    int rollbacks = 0;
    std::function<void()> captureXmlRollback() override {
        return [this, previous = value] {
            ++rollbacks;
            if (failRollback) throw std::runtime_error("injected rollback failure");
            value = previous;
        };
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
            "", "<loodGUI><InstrumentSetup schemaVersion='2'>new</InstrumentSetup><broken></loodGUI>",
            "<loodGUI><InstrumentSetup schemaVersion='2'>new</InstrumentSetup></loodGUI><extra/>"
        };
        for (const auto& document : malformed) {
            writeFile(path, document);
            const auto result = XmlConfigStore::loadAllFromXml(path, {&target});
            require(result.error == XmlOperationResult::Error::Parse, "malformed XML accepted");
            require(target.loads == 1 && target.value == source.value,
                    "malformed XML mutated page before validation");
        }
        writeFile(path, "<other><loodGUI><InstrumentSetup schemaVersion='2'>new</InstrumentSetup></loodGUI></other>");
        require(XmlConfigStore::loadAllFromXml(path, {&target}).error == XmlOperationResult::Error::InvalidRoot,
                "nested expected root should be rejected");
        require(target.loads == 1, "wrong root mutated page");

        writeFile(path, "<loodGUI><Future><InstrumentSetup schemaVersion='2'>hidden</InstrumentSetup></Future><InstrumentSetup schemaVersion='2'>visible</InstrumentSetup></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&target}).succeeded(), "unknown section accepted");
        require(target.loads == 1 && target.value == source.value, "unknown section changed data");
        writeFile(path, "<loodGUI/>");
        require(!XmlConfigStore::loadAllFromXml(path, {&target}).succeeded(), "empty root accepted");
        require(target.loads == 1, "empty root mutated page");
        TestPage first, second;
        second.tag = "TestConditions";
        first.value = "old-first";
        second.value = "old-second";
        writeFile(path, "<loodGUI><InstrumentSetup>new-first</InstrumentSetup></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded(), "missing page accepted");
        require(first.loads == 0 && second.loads == 0 && first.value == "old-first", "missing page changed data");
        second.failApply = true;
        writeFile(path, "<loodGUI><InstrumentSetup schemaVersion='2'>new-first</InstrumentSetup><TestConditions schemaVersion='2'>reject</TestConditions></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded(), "late apply failure accepted");
        require(first.value == "old-first" && second.value == "old-second", "late failure left a mixed configuration");
        require(first.publications == 0 && second.publications == 0, "failed transaction published changes");
        writeFile(path, "<loodGUI><TestConditions schemaVersion='2'>new-second</TestConditions><InstrumentSetup schemaVersion='2'>new-first</InstrumentSetup></loodGUI>");
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
        writeFile(path, "<loodGUI><InstrumentSetup schemaVersion='2'>one</InstrumentSetup><InstrumentSetup schemaVersion='2'>two</InstrumentSetup></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, &second}).succeeded(), "duplicate sections accepted");
        require(first.value == "new-first", "duplicate section changed current data");
        {
            CheckpointPage checkpoint;
            TestPage fallback;
            checkpoint.value = "checkpoint-old";
            fallback.tag = "TestConditions";
            fallback.value = "fallback-old";
            fallback.failApply = true;
            writeFile(path, "<loodGUI><InstrumentSetup>new</InstrumentSetup><TestConditions>reject</TestConditions></loodGUI>");
            auto result = XmlConfigStore::loadAllFromXml(path, {&checkpoint, &fallback});
            require(!result.succeeded() && !result.detail.contains("rollback failed") &&
                    checkpoint.value == "checkpoint-old" && checkpoint.loads == 1 && checkpoint.rollbacks == 1 &&
                    fallback.value == "fallback-old" && fallback.loads == 2,
                    "checkpoint and serialized rollback did not coexist");
            checkpoint.failRollback = true;
            result = XmlConfigStore::loadAllFromXml(path, {&checkpoint, &fallback});
            require(!result.succeeded() && result.detail.contains("rollback failed for InstrumentSetup") &&
                    checkpoint.rollbacks == 2 && fallback.value == "fallback-old" && fallback.loads == 4,
                    "checkpoint failure was hidden or prevented later page recovery");
        }
        auto* closing = new TestPage;
        closing->tag = "TestConditions";
        first.onPublish = [&] { delete closing; closing = nullptr; };
        writeFile(path, "<loodGUI><InstrumentSetup schemaVersion='2'>last</InstrumentSetup><TestConditions schemaVersion='2'>last</TestConditions></loodGUI>");
        require(!XmlConfigStore::loadAllFromXml(path, {&first, closing}).succeeded() && !closing,
                "notification dereferenced a closed page");
        std::cout << "PASS: round trip, atomic failure, open errors, preflight and section dispatch\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
