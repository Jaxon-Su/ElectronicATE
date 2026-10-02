#include "page2model.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        Page2Model model;
        model.setLoadMeta({{"CC"}, {"Low"}, {"first"}, {"12"}, {"1"}});
        model.setDynamicMeta({{"Low"}, {"12"}, {"1"}, {"10", "20"}});
        model.setRelayRows({{"relay", {"ON"}}});
        model.resizeLoadOutputs(3);
        require(model.getLoadMeta().names == QVector<QString>{"first", "", ""}, "load expansion lost data");
        require(model.getDynamicMeta().vo.size() == 3 && model.getDynamicMeta().t1t2 == QVector<QString>{"10", "20"}, "dynamic resize changed timing");
        model.resizeRelayOutputs(3);
        require(model.getRelayRows()[0].values == QVector<QString>{"ON", "off", "off"}, "relay default changed");
        model.resizeLoadOutputs(1);
        model.resizeRelayOutputs(1);
        require(model.getLoadMeta().ranges.size() == 1 && model.getRelayRows()[0].values.size() == 1, "output shrink failed");
        model.resizeLoadOutputs(0);
        require(model.getLoadMeta().names.size() == 1, "invalid count changed model");
        model.setInputRows({{"1phase", "110", "60", "0"}});
        int loaded = 0;
        QObject::connect(&model, &Page2Model::configLoaded, [&] { ++loaded; });
        QXmlStreamReader wrongRoot(QStringLiteral("<Other/>"));
        wrongRoot.readNextStartElement();
        model.loadXml(wrongRoot);
        require(wrongRoot.hasError() && loaded == 0 && model.getInputRows()[0].vin == "110",
                "wrong XML element replaced Page2 data");
        QXmlStreamReader broken(QStringLiteral("<Page2><InputTable><Row><Vin>230</Vin></Row></InputTable><LoadTable>"));
        broken.readNextStartElement();
        model.loadXml(broken);
        require(broken.hasError() && loaded == 0 && model.getInputRows()[0].vin == "110"
                    && model.getLoadMeta().names[0] == "first", "failed XML partially replaced model");
        QXmlStreamReader valid(QStringLiteral("<Page2><InputTable><Row><Vin>230</Vin></Row></InputTable></Page2>"));
        valid.readNextStartElement();
        model.loadXml(valid);
        require(!valid.hasError() && loaded == 1 && model.getInputRows()[0].vin == "230"
                    && model.getLoadRows().isEmpty(), "successful replacement failed");
        auto dc = model.snapshot();
        dc.dcRows = {{"24", "5", "main"}};
        dc.dcRows2 = {{"12", "3", "aux"}, {"10", "2", "low"}};
        dc.dcRows3 = {{"5", "1", "logic"}};
        dc.dcSourceTab = 2;
        model.setSnapshot(dc);
        QString xml;
        QXmlStreamWriter writer(&xml);
        model.writeXml(writer);
        Page2Model restored;
        QXmlStreamReader roundTrip(xml);
        roundTrip.readNextStartElement();
        restored.loadXml(roundTrip);
        const auto result = restored.snapshot();
        require(!roundTrip.hasError() && result.dcRows[0].currentLimit == "5"
                    && result.dcRows2.size() == 2 && result.dcRows2[1].label == "low"
                    && result.dcRows3[0].vin == "5" && result.dcSourceTab == 2,
                "three DC sources did not round trip");
        QXmlStreamReader legacy(QStringLiteral("<Page2><DcTable><Row><Vin>48</Vin></Row></DcTable></Page2>"));
        legacy.readNextStartElement();
        restored.loadXml(legacy);
        require(!legacy.hasError() && restored.snapshot().dcRows[0].vin == "48"
                    && restored.snapshot().dcRows[0].currentLimit.isEmpty()
                    && restored.snapshot().dcRows2.isEmpty() && restored.snapshot().dcRows3.isEmpty(),
                "legacy DC source migration failed");
        std::cout << "PASS: Page2 output updates and atomic XML replacement\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
