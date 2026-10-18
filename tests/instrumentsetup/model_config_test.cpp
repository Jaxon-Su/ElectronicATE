#include "xmlconfigstore.h"
#include "instrumentsetupmodel.h"
#include "testconditionsmodel.h"
#include "manualcontrolmodel.h"
#include "commandconsolemodel.h"
#include "testsequencemodel.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Model> class ModelPage : public IXmlSerializable {
public:
    Model model;
    explicit ModelPage(QString tag) : m_tag(std::move(tag)) {}
    QString xmlTagName() const override { return m_tag; }
    void writeXml(QXmlStreamWriter& writer) const override { model.writeXml(writer); }
    void validateXml(QXmlStreamReader& reader) const override { Model candidate; candidate.loadXml(reader); }
    void loadXml(QXmlStreamReader& reader) override { model.loadXml(reader); }
private:
    QString m_tag;
};

struct Configuration {
    ModelPage<InstrumentSetupModel> instrumentSetup{"InstrumentSetup"};
    ModelPage<TestConditionsModel> testConditions{"TestConditions"};
    ModelPage<ManualControlModel> manualControl{"ManualControl"};
    ModelPage<CommandConsoleModel> commandConsole{"CommandConsole"};
    ModelPage<TestSequenceModel> testSequence{"TestSequence"};
    QList<IXmlSerializable*> pages() { return {&instrumentSetup, &testConditions, &manualControl, &commandConsole, &testSequence}; }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        Configuration source;
        source.instrumentSetup.model.setLoadOutputs(2);
        source.instrumentSetup.model.setRelayOutputs(1);
        InstrumentConfig load;
        load.name = "Load1"; load.type = "Load"; load.modelName = "63600-5";
        load.commConfig = CommunicationConfig::fromResourceString("GPIB0::5::INSTR");
        load.channels.append({"63640-80-80", 1, 0});
        source.instrumentSetup.model.setInstrumentConfigs({load});
        source.testConditions.model.setInputRows({{"1phase", "110", "60", "0"}});
        source.testConditions.model.setDcRows({{"48"}});
        source.testConditions.model.setLoadMeta({{"CC", "CR"}, {"Low", "High"}, {"A", "B"}, {"12", "5"}, {"1", "1"}});
        source.testConditions.model.setLoadRows({{"load & one", {"2", "3"}}});
        source.testConditions.model.setDynamicMeta({{"Low"}, {"12"}, {"1"}, {"10", "20"}});
        source.testConditions.model.setDynamicRows({{"dynamic", {"1", "2"}}});
        source.testConditions.model.setRelayRows({{"relay", {"ON"}}});
        source.manualControl.model.setInputTitles({"1phase/110/60/0"});
        source.manualControl.model.setSelectedInputState(0, "1phase/110/60/0");
        source.manualControl.model.setTestConditionsLoadMetaDataChanged(source.testConditions.model.getLoadMeta());
        source.manualControl.model.setTestConditionsLoadRowsChanged(source.testConditions.model.getLoadRows());
        source.manualControl.model.setTestConditionsDynamicMetaChanged(source.testConditions.model.getDynamicMeta());
        source.manualControl.model.setTestConditionsDynamicRowsChanged(source.testConditions.model.getDynamicRows());
        source.manualControl.model.setTestConditionsRelayRowsChanged(source.testConditions.model.getRelayRows());
        source.commandConsole.model.setTimeout(12000);
        source.commandConsole.model.setAddressHistory({"GPIB0::5::INSTR"});
        source.commandConsole.model.setCommandHistory({"*IDN?"});
        source.commandConsole.model.addToFullHistory({"2026-09-13", "GPIB0::5::INSTR", "*IDN?", "offline", true});
        DutRowData row;
        row.item = "Delay";
        row.ext = QString::fromUtf8("測試 <A>");
        row.retry = "2";
        row.settings["delay"] = QVariantMap{{"delay_ms", 100}};
        source.testSequence.model.replaceTasks({row});

        const auto path = dir.filePath("all.xml");
        require(XmlConfigStore::saveAllToXml(path, source.pages()).succeeded(), "configuration save failed");
        Configuration restored;
        require(XmlConfigStore::loadAllFromXml(path, restored.pages()).succeeded(), "configuration load failed");
        require(restored.instrumentSetup.model.loadOutputs() == 2, "instrumentSetup lost");
        require(restored.testConditions.model.getInputRows().size() == 1 && restored.testConditions.model.getInputRows()[0].vin == "110", "input rows lost");
        require(restored.testConditions.model.getDcRows().size() == 1 && restored.testConditions.model.getDcRows()[0].vin == "48", "DC rows lost");
        require(restored.testConditions.model.getLoadRows().size() == 1 && restored.testConditions.model.getLoadRows()[0].values == QVector<QString>({"2", "3"}), "load rows lost");
        require(restored.testConditions.model.getDynamicMeta().t1t2 == source.testConditions.model.getDynamicMeta().t1t2, "dynamic timing lost");
        require(restored.testConditions.model.getRelayRows().size() == 1, "relay rows lost");
        require(restored.manualControl.model.getSelectedInputIndex() == 0
                    && restored.manualControl.model.getSelectedInputText() == "1phase/110/60/0", "manualControl selection lost");
        require(restored.manualControl.model.getLoadRowsData().size() == 1, "manualControl load snapshot lost");
        require(restored.commandConsole.model.timeout() == 12000 && restored.commandConsole.model.commandHistory() == QStringList{"*IDN?"}, "commandConsole settings lost");
        require(restored.commandConsole.model.fullHistory().size() == 1 && restored.commandConsole.model.fullHistory()[0].success, "commandConsole log lost");
        require(restored.testSequence.model.tasks().size() == 1 && restored.testSequence.model.tasks()[0].ext == row.ext, "testSequence rows lost");
        require(restored.testSequence.model.tasks()[0].settings.value("delay").toMap().value("delay_ms").toInt() == 100, "task settings lost");
        const auto snapshot = [](Configuration& config) {
            QString xml; QXmlStreamWriter writer(&xml);
            for (auto* page : config.pages()) page->writeXml(writer);
            return xml;
        };
        QFile saved(path); require(saved.open(QIODevice::ReadOnly), "saved XML read failed");
        const QString current = QString::fromUtf8(saved.readAll()); saved.close();
        const QString restoredBefore = snapshot(restored);
        const auto rejectsWithoutChanges = [&](const QString& document) {
            QFile fixture(path); require(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate), "fixture open failed");
            fixture.write(document.toUtf8()); fixture.close();
            require(!XmlConfigStore::loadAllFromXml(path, restored.pages()).succeeded(), "incomplete/old XML accepted");
            require(snapshot(restored) == restoredBefore, "rejected XML changed live configuration");
        };
        const QStringList sections{"InstrumentSetup", "TestConditions", "ManualControl", "CommandConsole", "TestSequence"};
        QString legacy = current;
        for (int i = 0; i < sections.size(); ++i) {
            const auto &section = sections[i];
            const QRegularExpression opening("<" + section + "(?=[\\s>])");
            require(opening.match(current).hasMatch(), "functional XML section missing");
            const QString oldTag = QString("Page%1").arg(i + 1);
            require(!current.contains("<" + oldTag), "numeric page tag written");
            QString mixed = current;
            mixed.replace(opening, "<" + oldTag);
            mixed.replace("</" + section + ">", "</" + oldTag + ">");
            rejectsWithoutChanges(mixed);
            legacy.replace(opening, "<" + oldTag);
            legacy.replace("</" + section + ">", "</" + oldTag + ">");
        }
        rejectsWithoutChanges(legacy);
        require(!current.contains("<WaveformOcr"), "OCR results unexpectedly persisted in configuration");
        for (const QString& tag : {"InstrumentSetup", "TestConditions", "ManualControl", "CommandConsole", "TestSequence", "LoadOutputs", "DcInputs",
                                  "DcConditions", "DcGroupSelection", "CurrentSelections", "Timeout",
                                  "FullHistory", "ReportFile", "Ext", "Settings", "CommunicationConfig"}) {
            QString incomplete = current;
            const QRegularExpression element("<" + tag + "\\b[^>]*(?:/>|>[\\s\\S]*?</" + tag + ">)");
            incomplete.remove(element);
            require(incomplete != current, "missing-field fixture did not change document");
            rejectsWithoutChanges(incomplete);
        }
        for (const QString& attribute : {"protocol", "timeout", "Report"}) {
            QString incomplete = current;
            incomplete.remove(QRegularExpression(" " + attribute + "=\"[^\"]*\""));
            require(incomplete != current, "missing-attribute fixture did not change document");
            rejectsWithoutChanges(incomplete);
        }
        QString unknown = current; unknown.replace("</loodGUI>", "<OldPage/></loodGUI>");
        rejectsWithoutChanges(unknown);
        // Direct CommandConsole loads are also transactional, including a missing required field.
        QString commandConsole; QXmlStreamWriter commandConsoleWriter(&commandConsole); restored.commandConsole.model.writeXml(commandConsoleWriter);
        commandConsole.remove(QRegularExpression("<Timeout>[^<]*</Timeout>"));
        QXmlStreamReader commandConsoleReader(commandConsole); commandConsoleReader.readNextStartElement(); restored.commandConsole.model.loadXml(commandConsoleReader);
        require(commandConsoleReader.hasError() && snapshot(restored) == restoredBefore, "CommandConsole missing-field load changed state");
        QFile invalid(path);
        require(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate), "fixture open failed");
        const QByteArray document = "<loodGUI><InstrumentSetup schemaVersion='2'><LoadOutputs>7</LoadOutputs></InstrumentSetup>"
            "<TestConditions schemaVersion='2'><InputTable><Row><Vin><unexpected/></Vin></Row></InputTable></TestConditions></loodGUI>";
        require(invalid.write(document) == document.size(), "fixture write failed");
        invalid.close();
        const auto rejected = XmlConfigStore::loadAllFromXml(path, restored.pages());
        require(rejected.error == XmlOperationResult::Error::Parse, "page parser error accepted");
        require(restored.instrumentSetup.model.loadOutputs() == 2, "later page error changed earlier model");
        require(restored.testConditions.model.getInputRows().size() == 1 && restored.testConditions.model.getInputRows()[0].vin == "110",
                "validation changed live input rows");
        std::cout << "PASS: real page round trip and isolated page validation\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
