#include "appservice.h"
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDebug>

AppService& AppService::instance()
{
    static AppService instance;
    return instance;
}

AppService::AppService(QObject* parent)
    : QObject(parent)
{
}

void AppService::setupPageConnections(Page1ViewModel* vm1,
                                      Page2ViewModel* vm2,
                                      Page3ViewModel* vm3)
{
    if (!vm1 || !vm2 || !vm3) {
        qWarning() << "AppService::setupPageConnections - null viewmodel pointer";
        return;
    }

    connectPage1ToPage2(vm1, vm2);
    connectPage1ToPage3(vm1, vm3);
    connectPage2ToPage3(vm2, vm3);
}

void AppService::connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2)
{
    // Page1 load 數量連動 Page2
    connect(vm1, &Page1ViewModel::loadOutputsChanged,
            vm2, &Page2ViewModel::setMaxOutput);

    connect(vm1, &Page1ViewModel::relayOutputsChanged,
            vm2, &Page2ViewModel::setMaxRelayOutput);
}

void AppService::connectPage1ToPage3(Page1ViewModel* vm1, Page3ViewModel* vm3)
{
    // Page1 連動 Page3
    connect(vm1, &Page1ViewModel::configUpdated,
            vm3, &Page3ViewModel::onPage1ConfigChanged);
}

void AppService::connectPage2ToPage3(Page2ViewModel* vm2, Page3ViewModel* vm3)
{
    // Page2 tblinput 連動 Page3
    connect(vm2, &Page2ViewModel::TitleListChanged,
            vm3, &Page3ViewModel::updateTitles);

    // Page2 tbl_input、tbl_load、tbl_Dynamic 連動 Page3
    connect(vm2, &Page2ViewModel::inputRowsStructChanged,
            vm3, &Page3ViewModel::onInputDataChanged);
    connect(vm2, &Page2ViewModel::loadMetaStructChanged,
            vm3, &Page3ViewModel::onLoadMetaChanged);
    connect(vm2, &Page2ViewModel::loadRowsStructChanged,
            vm3, &Page3ViewModel::onLoadRowsChanged);
    connect(vm2, &Page2ViewModel::dynamicMetaStructChanged,
            vm3, &Page3ViewModel::onDynamicMetaChanged);
    connect(vm2, &Page2ViewModel::dynamicRowsStructChanged,
            vm3, &Page3ViewModel::onDynamicRowsChanged);
}

void AppService::saveAllToXml(const QString& fileName,
                              Page1* page1, Page2* page2, Page3* page3,
                              Page1ViewModel* vm1, Page2ViewModel* vm2, Page3ViewModel* vm3)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << fileName;
        return;
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);

    writer.writeStartDocument();
    writer.writeStartElement("loodGUI");

    // 同步 UI 到 ViewModel
    if (page1) page1->syncUIToViewModel();
    if (page2) page2->syncUIToViewModel();
    if (page3) page3->syncUIToViewModel();

    // 寫入 XML
    if (vm1) vm1->writeXml(writer);
    if (vm2) vm2->writeXml(writer);
    if (vm3) vm3->writeXml(writer);

    writer.writeEndElement(); // loodGUI
    writer.writeEndDocument();
    file.close();
}

void AppService::loadAllFromXml(const QString& fileName,
                                Page1ViewModel* vm1, Page2ViewModel* vm2, Page3ViewModel* vm3)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for reading:" << fileName;
        return;
    }

    QXmlStreamReader reader(&file);

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (reader.name() == "Page1" && vm1) {
                vm1->loadXml(reader);
            }
            else if (reader.name() == "Page2" && vm2) {
                vm2->loadXml(reader);
            }
            else if (reader.name() == "Page3" && vm3) {
                vm3->loadXml(reader);
            }
        }
    }
    file.close();
}

void AppService::registerMetaTypes()
{
    qRegisterMetaType<LoadKind>("LoadKind");
}
