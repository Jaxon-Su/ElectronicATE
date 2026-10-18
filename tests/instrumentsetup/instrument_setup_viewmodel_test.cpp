#include "instrumentsetupviewmodel.h"
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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
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
