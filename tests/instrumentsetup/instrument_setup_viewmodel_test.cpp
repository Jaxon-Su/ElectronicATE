#include "instrumentsetupviewmodel.h"
#include "chroma6310frame.h"
#include <QCoreApplication>
#include <QPointer>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

QString currentXml(int outputs = 1)
{
    InstrumentSetupModel source; source.setLoadOutputs(outputs);
    QString xml; QXmlStreamWriter writer(&xml); source.writeXml(writer);
    return xml;
}

void checkLoadMainframes()
{
    InstrumentSetupModel model;
    InstrumentSetupViewModel vm(&model);
    require(model.loadBaseXml(INSTRUMENT_CATALOG), "missing shipped catalog");
    const auto old = vm.subModels("6314", 1), modern = vm.subModels("6314A", 1);
    require(old.size() == 16 && old.contains("63107") && old.contains("63107A") &&
            !old.contains("63110A") && !old.contains("63115A"), "old frame module choices");
    require(modern.size() == 13 && modern.contains("63123A-120") && !modern.contains("63103"),
            "A frame module choices");
    require(vm.subModels("6314A", 8) == QStringList{"63102A", "63107A", "63110A"} &&
            vm.subModels("6314A", 8, {{7, "63107A"}}) == QStringList{"63107A"} &&
            vm.subModels("6314A", 2, {{1, "63103A"}}).isEmpty(), "dual slot choices");
    require(vm.subModels("6314", 3, {{1, "63106"}}).isEmpty() &&
            !vm.subModels("6314", 7).contains("63106") &&
            vm.subModels("6314A", 7, {{1, "63112A"}}).isEmpty(), "wide module overlap");

    InstrumentConfig load;
    load.name = "Load1"; load.type = "Load"; load.modelName = "63601-5";
    load.commConfig = CommunicationConfig::fromResourceString("GPIB0::8::INSTR");
    load.channels.resize(8); // Old global columns: 1,2,3,4,5,7,9,10.
    load.channels[5] = {"63640-80-80", 1, 2};
    load.channels[7] = {"63610-80-20", 2};
    InstrumentSetupModel legacy;
    legacy.setInstrumentConfigs({load});
    QString oldXml; QXmlStreamWriter oldWriter(&oldXml); legacy.writeXml(oldWriter);
    QXmlStreamReader reader(oldXml); reader.readNextStartElement(); vm.loadXml(reader);
    const auto checkRestored = [&] {
        const auto restored = vm.currentConfig().instruments.first();
        require(restored.channels[restored.channelNumbers.indexOf(7)].index == 1 &&
                restored.channels[restored.channelNumbers.indexOf(7)].syncType == 2 &&
                restored.channels[restored.channelNumbers.indexOf(10)].index == 2 &&
                restored.channels[restored.channelNumbers.indexOf(6)].index == 0, "physical CH renumbered");
    };
    require(!reader.hasError(), "legacy channel XML rejected");
    checkRestored();
    int configUpdates = 0;
    QObject::connect(&vm, &InstrumentSetupViewModel::configUpdated, &vm, [&] { ++configUpdates; });
    vm.setLoadRowVisible("Load1", false);
    require(!vm.currentConfig().instruments.first().enabled && configUpdates == 0,
            "hiding a load must only update row visibility");
    checkRestored();
    auto duplicateLoad = vm.currentConfig().instruments.first();
    duplicateLoad.name = "Load2";
    duplicateLoad.channels[duplicateLoad.channelNumbers.indexOf(7)].index = 3;
    InstrumentSetupModel badCatalog;
    badCatalog.setInstrumentConfigs({vm.currentConfig().instruments.first(), duplicateLoad});
    QString badXml; QXmlStreamWriter out(&badXml); badCatalog.writeXml(out);
    require(out.hasError() && out.errorString().contains("duplicate"), "conflicting mappings were saved");
    duplicateLoad.commConfig.gpibAddress = 9;
    badCatalog.setInstrumentConfigs({vm.currentConfig().instruments.first(), duplicateLoad});
    badXml.clear();
    QXmlStreamWriter validOut(&badXml); badCatalog.writeXml(validOut);
    require(!validOut.hasError() && badXml.contains("<GpibAddress>9</GpibAddress>"), "independent chassis were rejected");
    badXml.replace("<GpibAddress>9</GpibAddress>","<GpibAddress>8</GpibAddress>");
    QXmlStreamReader conflict(badXml); conflict.readNextStartElement(); vm.loadXml(conflict);
    require(conflict.hasError() && vm.currentConfig().instruments.size()==1, "XML silently loaded duplicate physical CH");
    checkRestored();
    QString numbered; QXmlStreamWriter writer(&numbered); vm.writeXml(writer);
    require(numbered.contains("schemaVersion=\"3\"") && numbered.contains("number=\"7\""), "missing physical CH");
    QXmlStreamReader next(numbered); next.readNextStartElement(); vm.loadXml(next);
    require(!next.hasError(), "numbered channel XML rejected");
    checkRestored();
    auto missing = numbered;
    missing.remove("number=\"7\"");
    QXmlStreamReader partial(missing); partial.readNextStartElement(); vm.loadXml(partial);
    require(partial.hasError(), "missing physical CH accepted");
    numbered.replace("number=\"8\"", "number=\"7\"");
    QXmlStreamReader duplicate(numbered); duplicate.readNextStartElement(); vm.loadXml(duplicate);
    require(duplicate.hasError(), "duplicate physical CH accepted");

    load.modelName = "6314"; load.channelNumbers = {1}; load.channels = {{"63106", 1}};
    auto peer = load; peer.name = "Load2"; peer.channelNumbers = {3}; peer.channels = {{"63103", 2}};
    InstrumentSetupConfig shared; shared.instruments = {load, peer};
    require(!Chroma6310Frame::mappingError(shared, peer, 0).isEmpty(), "shared address slot overlap");
    shared.instruments[0].channels = {{"63107", 1}};
    shared.instruments[1].channels = {{"63107", 2}};
    shared.instruments[1].channelNumbers = {2};
    require(Chroma6310Frame::mappingError(shared, shared.instruments[1], 0).isEmpty(), "valid dual channel rejected");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        checkLoadMainframes();
        InstrumentSetupModel model;
        InstrumentSetupConfig config;
        config.loadOutputs = 7;
        InstrumentConfig instrument;
        instrument.name = "Load1";
        instrument.type = "Load";
        instrument.channels = {{"63101", 1, -1}};
        config.instruments = {instrument};
        model.setConfig(config);
        InstrumentSetupViewModel vm(&model);
        require(vm.loadOutputs() == 7 && vm.rows().size() == 1,
                "constructor did not hydrate supplied model");
        require(vm.currentConfig().instruments[0].channelNumbers == QList<int>{-1},
                "missing catalog mapping changed");
        int changes = 0;
        QObject::connect(&vm, &InstrumentSetupViewModel::loadOutputsChanged, [&](int) {
            require(model.relayOutputs() == 2 && model.getConfig().instruments.size() == 1,
                    "output signal exposed a partially committed configuration");
        });
        QObject::connect(&vm, &InstrumentSetupViewModel::configUpdated, [&](const InstrumentSetupConfig& value) {
            ++changes;
            require(value.loadOutputs == 3 && value.relayOutputs == 2, "published partial UI config");
        });
        vm.onUiConfigChanged({instrument}, 3, 2);
        require(changes == 1 && model.loadOutputs() == 3, "UI configuration not applied");
        QXmlStreamReader broken(QStringLiteral("<InstrumentSetup schemaVersion='2'><LoadOutputs>9</LoadOutputs>"));
        broken.readNextStartElement();
        vm.loadXml(broken);
        require(broken.hasError() && changes == 1 && model.loadOutputs() == 3, "failed load changed viewmodel");
        InstrumentSetupModel nestedModel;
        InstrumentSetupViewModel nested(&nestedModel);
        int finalUpdates = 0;
        QObject::connect(&nested, &InstrumentSetupViewModel::loadOutputsChanged, [&](int count) {
            if (count == 2) nested.onUiConfigChanged({}, 4, 5);
        });
        QObject::connect(&nested, &InstrumentSetupViewModel::configUpdated, [&](const InstrumentSetupConfig& value) {
            require(value.loadOutputs == 4 && value.relayOutputs == 5, "stale outer update published");
            ++finalUpdates;
        });
        nested.onUiConfigChanged({}, 2, 3);
        require(finalUpdates == 1 && nestedModel.loadOutputs() == 4, "reentrant update lost");
        InstrumentSetupModel loadedModel;
        InstrumentSetupViewModel loaded(&loadedModel);
        int loadedUpdates = 0;
        QObject::connect(&loaded, &InstrumentSetupViewModel::dataChanged, [&] {
            if (loadedModel.loadOutputs() == 2) {
                QXmlStreamReader newer(currentXml(8));
                newer.readNextStartElement();
                loaded.loadXml(newer);
            }
        });
        QObject::connect(&loaded, &InstrumentSetupViewModel::configUpdated, [&](const InstrumentSetupConfig& value) {
            require(value.loadOutputs == 8, "old XML load published after nested load");
            ++loadedUpdates;
        });
        QXmlStreamReader older(currentXml(2));
        older.readNextStartElement();
        loaded.loadXml(older);
        require(loadedUpdates == 1, "nested XML load published twice");
        InstrumentSetupModel deletedModel;
        auto* deleted = new InstrumentSetupViewModel(&deletedModel);
        QPointer<InstrumentSetupViewModel> alive(deleted);
        QObject::connect(deleted, &InstrumentSetupViewModel::dataChanged, [deleted] { delete deleted; });
        QXmlStreamReader document(currentXml());
        document.readNextStartElement();
        deleted->loadXml(document);
        require(alive.isNull() && !document.hasError(), "XML callback deletion failed");
        std::cout << "PASS: InstrumentSetup supplied model, atomic UI commit and reentrant update\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
