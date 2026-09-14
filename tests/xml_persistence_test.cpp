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

class TestPage : public IXmlSerializable {
public:
    QString value;
    int loads = 0;
    bool failWrite = false;
    QString xmlTagName() const override { return "Page1"; }
    void writeXml(QXmlStreamWriter& writer) const override
    {
        writer.writeTextElement(xmlTagName(), failWrite ? QString(QChar(1)) : value);
    }
    void validateXml(QXmlStreamReader& reader) const override { reader.readElementText(); }
    void loadXml(QXmlStreamReader& reader) override
    {
        ++loads;
        value = reader.readElementText();
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
        std::cout << "PASS: round trip, atomic failure, open errors, preflight and section dispatch\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
