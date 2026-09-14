#include "page1model.h"
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
        Page1Model model;
        model.setLoadOutputs(7);
        int updates = 0;
        QObject::connect(&model, &Page1Model::configLoaded, &app,
                         [&](const Page1Config&) { ++updates; });
        const QByteArray invalid[] = {
            "<Page1><LoadOutputs>2</LoadOutputs>",
            "<Page1><LoadOutputs><child/></LoadOutputs></Page1>",
            "<Page1><Instruments><Instrument><ModelName><child/></ModelName></Instrument></Instruments></Page1>",
            "<Page1><Instruments><Instrument><Channels><Channel/>",
            "<Page1><Instruments><Instrument><CommunicationConfig><Port><child/></Port></CommunicationConfig></Instrument></Instruments></Page1>",
            "<Other/>"
        };
        for (const auto& document : invalid) {
            QXmlStreamReader reader(document);
            reader.readNextStartElement();
            model.loadXml(reader);
            require(reader.hasError(), "invalid page was accepted");
            require(model.loadOutputs() == 7 && updates == 0, "invalid page changed model state");
        }
        const QByteArray valid =
            "<Page1><LoadOutputs>2</LoadOutputs><RelayOutputs>3</RelayOutputs><Instruments>"
            "<Instrument name='Load1' type='Load' enabled='true'><ModelName>63640</ModelName>"
            "<Address>GPIB0::5::INSTR</Address><Channels><Channel subModel='63600-5' index='3' syncType='SLAVE'/></Channels>"
            "</Instrument></Instruments></Page1>";
        QXmlStreamReader reader(valid);
        reader.readNextStartElement();
        model.loadXml(reader);
        require(!reader.hasError() && updates == 1, "valid load failed");
        require(model.loadOutputs() == 2 && model.relayOutputs() == 3, "output counts lost");
        const auto& config = model.getConfig();
        require(config.instruments.size() == 1, "instrument lost");
        const auto& instrument = config.instruments.first();
        require(instrument.channels.size() == 1 && instrument.channels.first().syncType == 2,
                "channel role lost");
        require(instrument.commConfig.protocol == ProtocolType::GPIB, "legacy address mapping lost");

        QByteArray saved;
        QXmlStreamWriter writer(&saved);
        model.writeXml(writer);
        Page1Model restored;
        QXmlStreamReader reload(saved);
        reload.readNextStartElement();
        restored.loadXml(reload);
        require(!reload.hasError() && restored.loadOutputs() == 2
                    && restored.getConfig().instruments.first().channels.first().index == 3,
                "model round trip failed");
        std::cout << "PASS: Page1 errors terminate without publishing partial state; model round trip preserved\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
