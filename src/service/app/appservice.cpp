#include "appservice.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "page4viewmodel.h"
#include "page5viewmodel.h"
#include "messageservice.h"
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
                                      Page3ViewModel* vm3,
                                      Page5ViewModel* vm5)
{
    if (!vm1 || !vm2 || !vm3 || !vm5) {
        qWarning() << "AppService::setupPageConnections - null viewmodel pointer";
        return;
    }

    connectPage1ToPage2(vm1, vm2);
    connectPage1ToPage3(vm1, vm3);
    connectPage2ToPage3(vm2, vm3);
    connectPage1ToPage5(vm1, vm5);
    connectPage2ToPage5(vm2, vm5);
}

void AppService::connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2)
{
    // Page1 load 數量連動 Page2
    connect(vm1, &Page1ViewModel::loadOutputsChanged,
            vm2, &Page2ViewModel::setMaxOutput);

    connect(vm1, &Page1ViewModel::relayOutputsChanged,
            vm2, &Page2ViewModel::setMaxRelayOutput);

    connect(vm1, &Page1ViewModel::configUpdated,
            vm2, &Page2ViewModel::onPage1ConfigChanged);
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
    connect(vm2, &Page2ViewModel::titleListChanged,
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
    connect(vm2, &Page2ViewModel::relayRowsStructChanged,
            vm3, &Page3ViewModel::onRelayRowsChanged);
}

void AppService::connectPage1ToPage5(Page1ViewModel* vm1, Page5ViewModel* vm5)
{
    // Page1 儀器通訊設定 連動 Page5
    connect(vm1, &Page1ViewModel::configUpdated,
            vm5, &Page5ViewModel::onPage1ConfigChanged);
}

void AppService::connectPage2ToPage5(Page2ViewModel* vm2, Page5ViewModel* vm5)
{
    // ★ 先注入委託指標，確保 Page5ViewModel 的 accessor 在 signal 抵達前已就緒
    vm5->setPage2ViewModel(vm2);

    // Page2 各條件 Table 連動 Page5（signal 只觸發 UI 刷新，不傳資料副本）
    connect(vm2, &Page2ViewModel::inputRowsStructChanged,
            vm5, &Page5ViewModel::onInputDataChanged);
    connect(vm2, &Page2ViewModel::loadMetaStructChanged,
            vm5, &Page5ViewModel::onLoadMetaChanged);
    connect(vm2, &Page2ViewModel::loadRowsStructChanged,
            vm5, &Page5ViewModel::onLoadRowsChanged);
    connect(vm2, &Page2ViewModel::dynamicMetaStructChanged,
            vm5, &Page5ViewModel::onDynamicMetaChanged);
    connect(vm2, &Page2ViewModel::dynamicRowsStructChanged,
            vm5, &Page5ViewModel::onDynamicRowsChanged);
    connect(vm2, &Page2ViewModel::relayRowsStructChanged,
            vm5, &Page5ViewModel::onRelayRowsChanged);
}

void AppService::saveAllToXml(const QString& fileName,
                              const QList<IXmlSerializable*>& pages)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        MessageService::instance().showError(
            tr("儲存失敗"), tr("無法開啟檔案: %1").arg(fileName));
        return;
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);
    writer.writeStartDocument();
    writer.writeStartElement("loodGUI");

    for (auto* page : pages) {
        if (page) page->writeXml(writer);
    }

    writer.writeEndElement(); // loodGUI
    writer.writeEndDocument();
    file.close();
}

void AppService::loadAllFromXml(const QString& fileName,
                                const QList<IXmlSerializable*>& pages)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        MessageService::instance().showError(
            tr("載入失敗"), tr("無法開啟檔案: %1").arg(fileName));
        return;
    }

    // 建立 tag → VM 的查詢表
    QMap<QString, IXmlSerializable*> pageMap;
    for (auto* page : pages) {
        if (page) pageMap[page->xmlTagName()] = page;
    }

    QXmlStreamReader reader(&file);

    // 找到根元素
    while (!reader.atEnd() && !reader.hasError()) {
        if (reader.readNext() == QXmlStreamReader::StartElement
            && reader.name() == QStringLiteral("loodGUI"))
            break;
    }

    if (reader.hasError()) {
        file.close();
        MessageService::instance().showError(
            tr("XML 解析錯誤"),
            tr("行 %1: %2").arg(reader.lineNumber()).arg(reader.errorString()));
        return;
    }

    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString tag = reader.name().toString();
            if (auto* vm = pageMap.value(tag, nullptr))
                vm->loadXml(reader);
        } else if (token == QXmlStreamReader::EndElement
                   && reader.name() == QStringLiteral("loodGUI")) {
            break;
        }
    }

    file.close();

    if (reader.hasError()) {
        MessageService::instance().showError(
            tr("XML 解析錯誤"),
            tr("行 %1: %2").arg(reader.lineNumber()).arg(reader.errorString()));
    }
}

void AppService::registerMetaTypes()
{
    qRegisterMetaType<TableKind>("TableKind");
}
