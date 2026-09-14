#include "page1viewmodel.h"
#include <QCoreApplication>
#include <QPointer>
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
        Page1Config config;
        config.loadOutputs = 7;
        InstrumentConfig instrument;
        instrument.name = "Load1";
        instrument.type = "Load";
        instrument.channels = {{"63101", 1, -1}};
        config.instruments = {instrument};
        model.setConfig(config);
        Page1ViewModel vm(&model);
        require(vm.loadOutputs() == 7 && vm.rows().size() == 1,
                "constructor did not hydrate supplied model");
        require(vm.currentConfig().instruments[0].channelNumbers == QList<int>{-1},
                "missing catalog mapping changed");
        int changes = 0;
        QObject::connect(&vm, &Page1ViewModel::loadOutputsChanged, [&](int) {
            require(model.relayOutputs() == 2 && model.getConfig().instruments.size() == 1,
                    "output signal exposed a partially committed configuration");
        });
        QObject::connect(&vm, &Page1ViewModel::configUpdated, [&](const Page1Config& value) {
            ++changes;
            require(value.loadOutputs == 3 && value.relayOutputs == 2, "published partial UI config");
        });
        vm.onUiConfigChanged({instrument}, 3, 2);
        require(changes == 1 && model.loadOutputs() == 3, "UI configuration not applied");
        QXmlStreamReader broken(QStringLiteral("<Page1><LoadOutputs>9</LoadOutputs>"));
        broken.readNextStartElement();
        vm.loadXml(broken);
        require(broken.hasError() && changes == 1 && model.loadOutputs() == 3, "failed load changed viewmodel");
        Page1Model nestedModel;
        Page1ViewModel nested(&nestedModel);
        int finalUpdates = 0;
        QObject::connect(&nested, &Page1ViewModel::loadOutputsChanged, [&](int count) {
            if (count == 2) nested.onUiConfigChanged({}, 4, 5);
        });
        QObject::connect(&nested, &Page1ViewModel::configUpdated, [&](const Page1Config& value) {
            require(value.loadOutputs == 4 && value.relayOutputs == 5, "stale outer update published");
            ++finalUpdates;
        });
        nested.onUiConfigChanged({}, 2, 3);
        require(finalUpdates == 1 && nestedModel.loadOutputs() == 4, "reentrant update lost");
        Page1Model loadedModel;
        Page1ViewModel loaded(&loadedModel);
        int loadedUpdates = 0;
        QObject::connect(&loaded, &Page1ViewModel::dataChanged, [&] {
            if (loadedModel.loadOutputs() == 2) {
                QXmlStreamReader newer(QStringLiteral("<Page1><LoadOutputs>8</LoadOutputs></Page1>"));
                newer.readNextStartElement();
                loaded.loadXml(newer);
            }
        });
        QObject::connect(&loaded, &Page1ViewModel::configUpdated, [&](const Page1Config& value) {
            require(value.loadOutputs == 8, "old XML load published after nested load");
            ++loadedUpdates;
        });
        QXmlStreamReader older(QStringLiteral("<Page1><LoadOutputs>2</LoadOutputs></Page1>"));
        older.readNextStartElement();
        loaded.loadXml(older);
        require(loadedUpdates == 1, "nested XML load published twice");
        Page1Model deletedModel;
        auto* deleted = new Page1ViewModel(&deletedModel);
        QPointer<Page1ViewModel> alive(deleted);
        QObject::connect(deleted, &Page1ViewModel::dataChanged, [deleted] { delete deleted; });
        QXmlStreamReader document(QStringLiteral("<Page1/>"));
        document.readNextStartElement();
        deleted->loadXml(document);
        require(alive.isNull() && !document.hasError(), "XML callback deletion failed");
        std::cout << "PASS: Page1 supplied model, atomic UI commit and reentrant update\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
