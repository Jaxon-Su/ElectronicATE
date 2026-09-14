#include "pageconnectioncoordinator.h"
#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "page5viewmodel.h"
#include <QDebug>

void PageConnectionCoordinator::setupPageConnections(Page1ViewModel* vm1,
                                      Page2ViewModel* vm2,
                                      Page3ViewModel* vm3,
                                      Page5ViewModel* vm5)
{
    if (!vm1 || !vm2 || !vm3 || !vm5) {
        qWarning() << "PageConnectionCoordinator::setupPageConnections - null viewmodel pointer";
        return;
    }

    connectPage1ToPage2(vm1, vm2);
    connectPage1ToPage3(vm1, vm3);
    connectPage2ToPage3(vm2, vm3);
    connectPage1ToPage5(vm1, vm5);
    connectPage2ToPage5(vm2, vm5);
}

void PageConnectionCoordinator::connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2)
{
    // Page1 load 數量連動 Page2
    QObject::connect(vm1, &Page1ViewModel::loadOutputsChanged,
            vm2, &Page2ViewModel::setMaxOutput);

    QObject::connect(vm1, &Page1ViewModel::relayOutputsChanged,
            vm2, &Page2ViewModel::setMaxRelayOutput);

    QObject::connect(vm1, &Page1ViewModel::configUpdated,
            vm2, &Page2ViewModel::onPage1ConfigChanged);
}

void PageConnectionCoordinator::connectPage1ToPage3(Page1ViewModel* vm1, Page3ViewModel* vm3)
{
    // Page1 連動 Page3
    QObject::connect(vm1, &Page1ViewModel::configUpdated,
            vm3, &Page3ViewModel::onPage1ConfigChanged);
}

void PageConnectionCoordinator::connectPage2ToPage3(Page2ViewModel* vm2, Page3ViewModel* vm3)
{
    // Page2 tblinput 連動 Page3
    QObject::connect(vm2, &Page2ViewModel::titleListChanged,
            vm3, &Page3ViewModel::updateTitles);

    QObject::connect(vm2, &Page2ViewModel::conditionsChanged,
                     vm3, &Page3ViewModel::onConditionsChanged);
}
void PageConnectionCoordinator::connectPage1ToPage5(Page1ViewModel* vm1, Page5ViewModel* vm5)
{
    // Page1 儀器通訊設定 連動 Page5
    QObject::connect(vm1, &Page1ViewModel::configUpdated,
            vm5, &Page5ViewModel::onPage1ConfigChanged);
}

void PageConnectionCoordinator::connectPage2ToPage5(Page2ViewModel* vm2, Page5ViewModel* vm5)
{
    // ★ 先注入委託指標，確保 Page5ViewModel 的 accessor 在 signal 抵達前已就緒
    vm5->setConditionProvider(vm2);

    // Page2 各條件 Table 連動 Page5（signal 只觸發 UI 刷新，不傳資料副本）
    QObject::connect(vm2, &Page2ViewModel::inputRowsStructChanged,
            vm5, &Page5ViewModel::onInputDataChanged);
    QObject::connect(vm2, &Page2ViewModel::loadMetaStructChanged,
            vm5, &Page5ViewModel::onLoadMetaChanged);
    QObject::connect(vm2, &Page2ViewModel::loadRowsStructChanged,
            vm5, &Page5ViewModel::onLoadRowsChanged);
    QObject::connect(vm2, &Page2ViewModel::dynamicMetaStructChanged,
            vm5, &Page5ViewModel::onDynamicMetaChanged);
    QObject::connect(vm2, &Page2ViewModel::dynamicRowsStructChanged,
            vm5, &Page5ViewModel::onDynamicRowsChanged);
    QObject::connect(vm2, &Page2ViewModel::relayRowsStructChanged,
            vm5, &Page5ViewModel::onRelayRowsChanged);
}
