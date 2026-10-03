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
            "<Page1 schemaVersion='2'><LoadOutputs>2</LoadOutputs>",
            "<Page1 schemaVersion='2'><LoadOutputs><child/></LoadOutputs></Page1>",
            "<Page1 schemaVersion='2'><Instruments><Instrument><ModelName><child/></ModelName></Instrument></Instruments></Page1>",
            "<Page1 schemaVersion='2'><Instruments><Instrument><Channels><Channel/>",
            "<Page1 schemaVersion='2'><Instruments><Instrument><CommunicationConfig><Port><child/></Port></CommunicationConfig></Instrument></Instruments></Page1>",
            "<Page1 schemaVersion='2'><DcInputs>1</DcInputs><DcInputs>2</DcInputs></Page1>",
            "<Page1 schemaVersion='2'><LoadOutputs>0</LoadOutputs></Page1>",
            "<Page1 schemaVersion='2'><Instruments><Instrument><Address>GPIB0::1::INSTR</Address></Instrument></Instruments></Page1>",
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
            "<Page1 schemaVersion='2'><LoadOutputs>2</LoadOutputs><RelayOutputs>3</RelayOutputs><Instruments>"
            "<Instrument name='Load1' type='Load' enabled='true'><ModelName>63640</ModelName>"
            "<CommunicationConfig protocol='GPIB' timeout='5000'><GpibBoard>0</GpibBoard><GpibAddress>5</GpibAddress><GpibSecondary>0</GpibSecondary></CommunicationConfig><Channels><Channel subModel='63600-5' index='3' syncType='SLAVE'/></Channels>"
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
        require(instrument.commConfig.protocol == ProtocolType::GPIB, "structured communication configuration lost");

        QByteArray saved;
        QXmlStreamWriter writer(&saved);
        model.writeXml(writer);
        require(!saved.contains("<Address>") && saved.contains("CommunicationConfig"), "old Address XML still written");
        Page1Model restored;
        QXmlStreamReader reload(saved);
        reload.readNextStartElement();
        restored.loadXml(reload);
        require(!reload.hasError() && restored.loadOutputs() == 2
                    && restored.getConfig().instruments.first().channels.first().index == 3,
                "model round trip failed");
        QXmlStreamReader legacyDc(QStringLiteral("<Page1 schemaVersion='2'><Instruments><Instrument name='DCSource' type='InputDCSource' enabled='true'><ModelName>62000P</ModelName><Address>GPIB0::8::INSTR</Address></Instrument></Instruments></Page1>"));
        legacyDc.readNextStartElement();
        restored.loadXml(legacyDc);
        require(legacyDc.hasError(), "old DC alias must be rejected");
        Page1Config dcConfig;
        for (int source = 1; source <= 3; ++source) {
            InstrumentConfig instrument;
            instrument.name = QString("DC Source%1").arg(source);
            instrument.type = "InputDCSource";
            instrument.address = QString("GPIB0::%1::INSTR").arg(source + 7);
            dcConfig.instruments.append(instrument);
        }
        restored.setInstrumentConfigs(dcConfig.instruments);
        auto counted = restored.getConfig();
        counted.dcInputs = 1;
        restored.setConfig(counted);
        QString dcXml;
        QXmlStreamWriter dcWriter(&dcXml);
        restored.writeXml(dcWriter);
        QXmlStreamReader dcReader(dcXml);
        dcReader.readNextStartElement();
        restored.loadXml(dcReader);
        require(!dcReader.hasError() && restored.getConfig().instruments[1].address == "GPIB0::9::INSTR"
                    && restored.getConfig().instruments[2].address == "GPIB0::10::INSTR",
                "DC instrument addresses did not round trip");
        require(restored.getConfig().dcInputs == 1, "DC count round trip");
        QXmlStreamReader invalidCount("<Page1 schemaVersion='2'><DcInputs>4</DcInputs></Page1>");
        invalidCount.readNextStartElement(); restored.loadXml(invalidCount);
        require(invalidCount.hasError() && restored.getConfig().dcInputs == 1, "invalid count changed config");
        std::cout << "PASS: Page1 errors terminate without publishing partial state; model round trip preserved\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
