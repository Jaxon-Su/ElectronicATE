#include "page2viewmodel.h"
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
        Page2Model model;
        Page2ViewModel vm(&model);
        TestConditionSnapshot delivered;
        int snapshots = 0;
        QObject::connect(&vm, &Page2ViewModel::conditionsChanged, &app,
                         [&](const TestConditionSnapshot& value) { delivered = value; ++snapshots; },
                         Qt::QueuedConnection);
        int headers = 0, metaUpdates = 0;
        QObject::connect(&vm, &Page2ViewModel::headersChanged, [&](TableKind, const QStringList&) { ++headers; });
        QObject::connect(&vm, &Page2ViewModel::loadMetaStructChanged, [&](const LoadMetaRow&) { ++metaUpdates; });
        vm.setLoadMeta({{"CC"}, {"Auto Range"}, {"output"}, {"12"}, {"1"}});
        vm.setLoadRows({{"condition", {"2"}}});
        QCoreApplication::processEvents();
        require(snapshots == 2 && delivered.loadMeta.vo[0] == "12" && delivered.loadRows[0].values[0] == "2",
                "condition snapshot was incomplete or not queueable");
        const auto saved = delivered;
        vm.setInputRows({{"1phase", "110", "60", "0"}});
        QCoreApplication::processEvents();
        require(saved.inputRows.isEmpty() && delivered.inputRows.size() == 1, "published snapshot changed later");
        vm.setMaxOutput(1);
        require(vm.calcRowPower(0) == 24.0, "model update lost power calculation");
        const int before = headers;
        vm.setMaxOutput(1);
        require(headers == before, "same output count emitted new headers");
        vm.setMaxOutput(2);
        require(headers == before + 2 && vm.loadMeta().names.size() == 2, "resize was not propagated");
        const int beforeFailure = metaUpdates;
        QXmlStreamReader broken(QStringLiteral("<Page2><LoadTable>"));
        broken.readNextStartElement();
        vm.loadXml(broken);
        require(broken.hasError() && metaUpdates == beforeFailure && vm.loadRows().size() == 1,
                "failed XML caused view updates");
        Page1Config config;
        InstrumentConfig load;
        load.type = "Load";
        load.channels = {{"63101A", 1, -1}};
        config.instruments = {load};
        vm.onPage1ConfigChanged(config);
        require(vm.loadRangeOptions(1, "CC") == QStringList{"Auto Range", "No Setting", "CCL", "CCH"},
                "configured load capability not exposed");
        require(vm.dynamicRangeOptions(1) == QStringList{"Auto Range", "No Setting", "CCDL", "CCDH"},
                "dynamic load capability not exposed");
        config.instruments[0].enabled = false;
        vm.onPage1ConfigChanged(config);
        require(vm.loadRangeOptions(1, "CC") == QStringList{"Auto Range", "No Setting"},
                "disabled load supplied capabilities");
        Page2Model nestedModel;
        Page2ViewModel nested(&nestedModel);
        int rowSignals = 0;
        QObject::connect(&nested, &Page2ViewModel::conditionsChanged, [&](const TestConditionSnapshot& value) {
            if (value.inputRows[0].vin == "110") nested.setInputRows({{"1phase", "230", "60", "0"}});
        });
        QObject::connect(&nested, &Page2ViewModel::inputRowsStructChanged, [&](const QVector<InputRow>& rows) {
            require(rows[0].vin == "230", "stale granular notification followed newer snapshot");
            ++rowSignals;
        });
        nested.setInputRows({{"1phase", "110", "60", "0"}});
        require(rowSignals == 1, "nested snapshot notification count changed");
        Page2Model batchModel;
        Page2ViewModel batch(&batchModel);
        int batches = 0;
        QObject::connect(&batch, &Page2ViewModel::conditionsChanged, [&](const auto&) { ++batches; });
        QObject::connect(&batch, &Page2ViewModel::inputRowsStructChanged, [&](const auto&) {
            require(batch.loadMeta().vo == QVector<QString>{"24"}
                        && batch.loadRows()[0].values == QVector<QString>{"3"}, "batch exposed partial load data");
        });
        TestConditionSnapshot batchValue;
        batchValue.inputRows = {{"1phase", "230", "50", "0"}};
        batchValue.loadMeta = {{"CC"}, {"Auto Range"}, {"out"}, {"24"}, {"1"}};
        batchValue.loadRows = {{"load", {"3"}}};
        batch.setConditions(batchValue);
        require(batches == 1 && batch.calcRowPower(0) == 72, "batch was split or power used stale meta");
        Page2Model deletedModel;
        auto* deleted = new Page2ViewModel(&deletedModel);
        QPointer<Page2ViewModel> deletedAlive(deleted);
        QObject::connect(deleted, &Page2ViewModel::dataChanged, [deleted] { delete deleted; });
        QXmlStreamReader validLoad(QStringLiteral("<Page2/>"));
        validLoad.readNextStartElement();
        deleted->loadXml(validLoad);
        require(deletedAlive.isNull() && !validLoad.hasError(), "load notification lifetime failed");
        std::cout << "PASS: Page2 ViewModel without Widgets or concrete drivers\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
