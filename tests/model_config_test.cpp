#include "xmlconfigstore.h"
#include "page1model.h"
#include "page2model.h"
#include "page3model.h"
#include "page4model.h"
#include "page5model.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
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
    ModelPage<Page1Model> p1{"Page1"};
    ModelPage<Page2Model> p2{"Page2"};
    ModelPage<Page3Model> p3{"Page3"};
    ModelPage<Page4Model> p4{"Page4"};
    ModelPage<Page5Model> p5{"Page5"};
    QList<IXmlSerializable*> pages() { return {&p1, &p2, &p3, &p4, &p5}; }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        Configuration source;
        source.p1.model.setLoadOutputs(2);
        source.p1.model.setRelayOutputs(1);
        source.p2.model.setInputRows({{"1phase", "110", "60", "0"}});
        source.p2.model.setDcRows({{"48"}});
        source.p2.model.setLoadMeta({{"CC", "CR"}, {"Low", "High"}, {"A", "B"}, {"12", "5"}, {"1", "1"}});
        source.p2.model.setLoadRows({{"load & one", {"2", "3"}}});
        source.p2.model.setDynamicMeta({{"Low"}, {"12"}, {"1"}, {"10", "20"}});
        source.p2.model.setDynamicRows({{"dynamic", {"1", "2"}}});
        source.p2.model.setRelayRows({{"relay", {"ON"}}});
        source.p3.model.setInputTitles({"1phase/110/60/0"});
        source.p3.model.setSelectedInputState(0, "1phase/110/60/0");
        source.p3.model.setPage2LoadMetaDataChanged(source.p2.model.getLoadMeta());
        source.p3.model.setPage2LoadRowsChanged(source.p2.model.getLoadRows());
        source.p3.model.setPage2DynamicMetaChanged(source.p2.model.getDynamicMeta());
        source.p3.model.setPage2DynamicRowsChanged(source.p2.model.getDynamicRows());
        source.p3.model.setPage2RelayRowsChanged(source.p2.model.getRelayRows());
        source.p4.model.setTimeout(12000);
        source.p4.model.setAddressHistory({"GPIB0::5::INSTR"});
        source.p4.model.setCommandHistory({"*IDN?"});
        source.p4.model.addToFullHistory({"2026-09-13", "GPIB0::5::INSTR", "*IDN?", "offline", true});
        DutRowData row;
        row.item = "Delay";
        row.ext = QString::fromUtf8("測試 <A>");
        row.retry = "2";
        row.settings["delay"] = QVariantMap{{"delay_ms", 100}};
        source.p5.model.dutRows = {row};

        const auto path = dir.filePath("all.xml");
        require(XmlConfigStore::saveAllToXml(path, source.pages()).succeeded(), "configuration save failed");
        Configuration restored;
        require(XmlConfigStore::loadAllFromXml(path, restored.pages()).succeeded(), "configuration load failed");
        require(restored.p1.model.loadOutputs() == 2, "page1 lost");
        require(restored.p2.model.getInputRows().size() == 1 && restored.p2.model.getInputRows()[0].vin == "110", "input rows lost");
        require(restored.p2.model.getDcRows().size() == 1 && restored.p2.model.getDcRows()[0].vin == "48", "DC rows lost");
        require(restored.p2.model.getLoadRows().size() == 1 && restored.p2.model.getLoadRows()[0].values == QVector<QString>({"2", "3"}), "load rows lost");
        require(restored.p2.model.getDynamicMeta().t1t2 == source.p2.model.getDynamicMeta().t1t2, "dynamic timing lost");
        require(restored.p2.model.getRelayRows().size() == 1, "relay rows lost");
        require(restored.p3.model.getSelectedInputIndex() == 0
                    && restored.p3.model.getSelectedInputText() == "1phase/110/60/0", "page3 selection lost");
        require(restored.p3.model.getLoadRowsData().size() == 1, "page3 load snapshot lost");
        require(restored.p4.model.timeout() == 12000 && restored.p4.model.commandHistory() == QStringList{"*IDN?"}, "page4 settings lost");
        require(restored.p4.model.fullHistory().size() == 1 && restored.p4.model.fullHistory()[0].success, "page4 log lost");
        require(restored.p5.model.dutRows.size() == 1 && restored.p5.model.dutRows[0].ext == row.ext, "page5 rows lost");
        require(restored.p5.model.dutRows[0].settings.value("delay").toMap().value("delay_ms").toInt() == 100, "task settings lost");
        QFile invalid(path);
        require(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate), "fixture open failed");
        const QByteArray document = "<loodGUI><Page1><LoadOutputs>7</LoadOutputs></Page1>"
            "<Page2><InputTable><Row><Vin><unexpected/></Vin></Row></InputTable></Page2></loodGUI>";
        require(invalid.write(document) == document.size(), "fixture write failed");
        invalid.close();
        const auto rejected = XmlConfigStore::loadAllFromXml(path, restored.pages());
        require(rejected.error == XmlOperationResult::Error::Parse, "page parser error accepted");
        require(restored.p1.model.loadOutputs() == 2, "later page error changed earlier model");
        require(restored.p2.model.getInputRows().size() == 1 && restored.p2.model.getInputRows()[0].vin == "110",
                "validation changed live input rows");
        std::cout << "PASS: real page round trip and isolated page validation\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
