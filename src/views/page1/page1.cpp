#include "page1.h"
#include "page1viewmodel.h"
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QMap>
#include <QEvent>

/* ──────────────────────────────────────────────────────────────── */
Page1::Page1(Page1ViewModel* vm, QWidget *parent) : QWidget(parent), viewModel(vm)
{
    initializeUI();
    setupLayout();
    setupTable();
    setupCheckboxes();
    setupConnections();

}

Page1::~Page1() = default;

void Page1::initializeUI()
{
    // 建立各 UI 元件（不要安排 Layout）
    tableWidget = new QTableWidget(this);

    spinBox_Load_Outputs = new QSpinBox(this);
    spinBox_Load_Outputs->setObjectName("spinBox_Load_Outputs");
    spinBox_Load_Outputs->setRange(1, 20);
    spinBox_Load_Outputs->setValue(1);

    spinBox_Relay_Outputs = new QSpinBox(this);
    spinBox_Relay_Outputs->setObjectName("spinBox_Relay_Outputs");
    spinBox_Relay_Outputs->setRange(1, 20);
    spinBox_Relay_Outputs->setValue(1);

    groupBox = new QGroupBox("Select Modules", this);
    QFont groupFont = groupBox->font();
    groupFont.setBold(true);
    groupBox->setFont(groupFont);

    checkboxLayout = new QVBoxLayout(groupBox);
    checkboxLayout->setSpacing(5);
    checkboxLayout->setContentsMargins(5, 5, 5, 5);

    groupBox->setStyleSheet(R"(
        QGroupBox { margin-top:0px; padding-top:4px; border:1px solid lightgray; }
        QGroupBox::title { subcontrol-origin:content; subcontrol-position:top left; padding:2px 5px; }
    )");
}

void Page1::setupCheckboxes(const QMap<QString, bool> &enabledMap)
{
    // 如果已有 checkbox,只更新狀態
    if (!instrumentCheckboxes.isEmpty()) {
        for (auto it = instrumentCheckboxes.begin(); it != instrumentCheckboxes.end(); ++it) {
            bool checked = enabledMap.value(it.key(), true);
            it.value()->blockSignals(true);
            it.value()->setChecked(checked);
            it.value()->blockSignals(false);
        }
        return;  // 直接返回,不重建
    }

    // 首次建立時才執行下面的代碼
    checkboxLayout->addSpacing(16);

    for (const auto &r : viewModel->rows()) {
        auto *cb = new QCheckBox(r.instrument, this);
        bool checked = enabledMap.value(r.instrument, true);
        cb->setChecked(checked);
        checkboxLayout->addWidget(cb);
        instrumentCheckboxes[r.instrument] = cb;
        connect(cb, &QCheckBox::checkStateChanged, this, &Page1::onInstrumentToggled);
    }
}

void Page1::setupLayout()
{
    QLabel *label1 = new QLabel("  Load Outputs", this);
    QFont boldFont = label1->font();
    boldFont.setBold(true);
    label1->setFont(boldFont);

    QLabel *label2 = new QLabel("  Relay Outputs", this);
    label2->setFont(boldFont);

    // Load Output + SpinBox 一組
    QVBoxLayout *loadSpinLayout = new QVBoxLayout;
    loadSpinLayout->setSpacing(2);
    loadSpinLayout->setContentsMargins(0, 0, 0, 0);
    loadSpinLayout->addWidget(label1);
    loadSpinLayout->addWidget(spinBox_Load_Outputs);

    QWidget *loadSpinWidget = new QWidget(this);
    loadSpinWidget->setLayout(loadSpinLayout);
    loadSpinWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Relay Output + SpinBox 一組
    QVBoxLayout *relaySpinLayout = new QVBoxLayout;
    relaySpinLayout->setSpacing(2);
    relaySpinLayout->setContentsMargins(0, 0, 0, 0);
    relaySpinLayout->addWidget(label2);
    relaySpinLayout->addWidget(spinBox_Relay_Outputs);

    QWidget *relaySpinWidget = new QWidget(this);
    relaySpinWidget->setLayout(relaySpinLayout);
    relaySpinWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QWidget *leftContainer = new QWidget(this);
    QVBoxLayout *leftInnerLayout = new QVBoxLayout(leftContainer);
    leftInnerLayout->setSpacing(6);
    leftInnerLayout->setContentsMargins(0, 0, 0, 0);
    leftInnerLayout->addWidget(loadSpinWidget);
    leftInnerLayout->addWidget(relaySpinWidget);
    leftInnerLayout->addWidget(groupBox);

    QVBoxLayout *mainLeftLayout = new QVBoxLayout;
    mainLeftLayout->setSpacing(0);
    mainLeftLayout->setContentsMargins(5, 5, 5, 5);
    mainLeftLayout->addWidget(leftContainer, 0, Qt::AlignTop);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(mainLeftLayout,1);
    mainLayout->addWidget(tableWidget,5);
    mainLayout->setContentsMargins(8,8,8,8);
    mainLayout->setSpacing(12);

    setLayout(mainLayout);
}

void Page1::setupConnections()
{
    connect(spinBox_Load_Outputs, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Page1::syncUIToViewModel);

    connect(viewModel, &Page1ViewModel::loadOutputsChanged,
            this,      &Page1::applyLoadOutputs);

    connect(viewModel, &Page1ViewModel::relayOutputsChanged,
            this,      &Page1::applyRelayOutputs);

    connect(spinBox_Relay_Outputs, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Page1::syncUIToViewModel);

    connect(viewModel, &Page1ViewModel::dataChanged,
            this,      &Page1::resetUIFromViewModel);

    connect(this, &Page1::uiConfigChanged, viewModel, &Page1ViewModel::onUiConfigChanged);

}

// ==================== page1.cpp 主函式優化 ====================

void Page1::setupTable()
{
    tableWidget->clear();
    tableWidget->setRowCount(0);

    prepareTableStructure();
    buildConfigMap();

    int rowIdx = 0;
    for (const auto &r : viewModel->rows()) {
        rowIdx = setupInstrumentRow(rowIdx, r);
    }

    finalizeTable();
}

// ==================== 輔助函式 ====================

void Page1::prepareTableStructure()
{
    // qDebug() << "viewModel->channels() = " << viewModel->channels();
    const auto channels = viewModel->channels();
    const int nCols = 3 + channels.size();

    int totalRows = 0;
    for (const auto &r : viewModel->rows())
        totalRows += r.hasChannels ? 2 : 1;

    tableWidget->clear();
    tableWidget->setColumnCount(nCols);
    tableWidget->setRowCount(totalRows);

    // 設定表頭
    QStringList hdr{ "Instrument", "Model", "Address" };
    for (int ch : channels)
        hdr << QString("Ch%1").arg(ch);

    tableWidget->setHorizontalHeaderLabels(hdr);
    tableWidget->verticalHeader()->setVisible(false); // 隱藏表格最左側數字
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); //讓所有列(columns)自動延展填滿整個表格寬度。
}

void Page1::buildConfigMap()
{
    m_configMap.clear();
    m_instrumentWidgets.clear();
    m_instrumentTypeCache.clear();

    //ic是struct結構體，參照Page1Config
    for (const auto &ic : viewModel->currentConfig().instruments){
        // qDebug() << "ic.name = " << ic.name;
        m_configMap[ic.name] = ic;
    }

    //快速查詢儀器type
    for (const auto &r : viewModel->rows()) {
        m_instrumentTypeCache[r.instrument] = r.type;
    }
}

int Page1::setupInstrumentRow(int startRow, const TableRowInfo &rowInfo)
{
    // T QMap::value(const Key &key, const T &defaultValue = T()) const
    // 第一個參數: rowInfo.instrument - 要查找的鍵(儀器名稱,如 "Load1")
    // 第二個參數: InstrumentConfig() - 找不到時返回的預設值(臨時建立的空結構體)
    InstrumentConfig ic = m_configMap.value(rowInfo.instrument, InstrumentConfig());

    setupNameColumn(startRow, rowInfo.instrument); // Load1、Load2.......
    setupBasicColumns(startRow, rowInfo, ic);   // model name、Address

    if (rowInfo.hasChannels)
        return setupChannelRow(startRow, rowInfo, ic); // Load、Relay comboBox
    else
        return setupSimpleRow(startRow); // 非Load、Relay comboBox
}

void Page1::setupNameColumn(int row, const QString &name)
{
    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignCenter);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    tableWidget->setItem(row, 0, nameItem);
}

void Page1::setupBasicColumns(int row, const TableRowInfo &rowInfo, const InstrumentConfig &ic)
{
    RowWidgets widgets;

    // Model ComboBox
    widgets.modelCb = createModelComboBox(rowInfo.modelCandidates, ic.modelName);
    tableWidget->setCellWidget(row, 1, widgets.modelCb);

    // Address ComboBox
    widgets.addrCb = createAddressComboBox(ic.address);
    tableWidget->setCellWidget(row, 2, widgets.addrCb);

    m_instrumentWidgets[rowInfo.instrument] = widgets;
}

int Page1::setupChannelRow(int startRow, const TableRowInfo &rowInfo, const InstrumentConfig &ic)
{
    // 合併前三欄
    tableWidget->setSpan(startRow, 0, 2, 1);
    tableWidget->setSpan(startRow, 1, 2, 1);
    tableWidget->setSpan(startRow, 2, 2, 1);

    QSet<int> validChannels = viewModel->channelsOfModel(ic.modelName);
    QStringList validSubModels = viewModel->subModels(ic.modelName);
    const auto channels = viewModel->channels();

    RowWidgets &widgets = m_instrumentWidgets[rowInfo.instrument];

    for (int c = 0; c < channels.size(); ++c) {
        int col = 3 + c;
        bool chEnabled = validChannels.contains(channels[c]);

        // 上排 subModel
        auto *subModelCb = createSubModelComboBox(ic, c, chEnabled, validSubModels);
        widgets.subModelCbs.append(subModelCb);
        tableWidget->setCellWidget(startRow, col, subModelCb);

        // 下排 index
        auto *indexCb = createIndexComboBox(ic, c, rowInfo.type, chEnabled);
        widgets.indexCbs.append(indexCb);
        tableWidget->setCellWidget(startRow + 1, col, indexCb);
    }

    // 連接 model 變更事件
    connectModelChangeHandler(startRow, rowInfo, widgets.modelCb);

    return startRow + 2;
}

int Page1::setupSimpleRow(int startRow)
{
    const int nCols = tableWidget->columnCount();
    for (int c = 3; c < nCols; ++c) {
        auto *empty = new QTableWidgetItem("");
        empty->setFlags(empty->flags() & ~Qt::ItemIsEditable);
        empty->setTextAlignment(Qt::AlignCenter);
        tableWidget->setItem(startRow, c, empty);
    }
    return startRow + 1;
}

void Page1::finalizeTable()
{
    enforceUniqueIndices(); //保持index唯一性
    refreshIndexChoices();  //若已經選擇index，其他地方下拉選單會呈現灰色

    for (auto it = instrumentCheckboxes.cbegin(); it != instrumentCheckboxes.cend(); ++it)
        updateInstrumentVisibility(it.key(), it.value()->isChecked());
}

// ==================== Widget 創建函式 ====================

QComboBox* Page1::createModelComboBox(const QStringList &candidates, const QString &saved)
{
    auto *cb = new QComboBox(this);
    cb->addItem("");
    cb->addItems(candidates);

    if (!saved.isEmpty())
        cb->setCurrentText(saved);
    else
        cb->setCurrentIndex(0);

    applyComboBoxStyle(cb);
    autoSyncOnWidgetChanged(cb);
    return cb;
}

QComboBox* Page1::createAddressComboBox(const QString &saved)
{
    auto *cb = new QComboBox(this);
    // cb->blockSignals(true);
    cb->setEditable(true);

    // 添加選項 0-30
    for (int i = 0; i <= 30; ++i)
        cb->addItem(i == 0 ? QString() : QString::number(i));

    // 設置文字對齊
    if (cb->lineEdit())
        cb->lineEdit()->setAlignment(Qt::AlignCenter);

    // 修正：使用 findText 和 setCurrentIndex 組合
    if (!saved.isEmpty()) {
        int index = cb->findText(saved);
        if (index >= 0) {
            // 如果在列表中找到，使用 setCurrentIndex
            cb->setCurrentIndex(index);
        } else {
            // 如果不在列表中（可編輯的，可能有自定義值），使用 setCurrentText
            cb->setCurrentText(saved);
        }

    } else {
        // 如果沒有保存值，設置為空（index 0）
        cb->setCurrentIndex(0);
    }
    // cb->blockSignals(false);

    applyComboBoxStyle(cb, true);
    autoSyncOnWidgetChanged(cb);
    return cb;
}

QComboBox* Page1::createSubModelComboBox(const InstrumentConfig &ic, int channelIdx,
                                         bool enabled, const QStringList &validSubModels)
{
    auto *cb = new QComboBox(this);
    cb->addItem("");

    if (enabled)
        cb->addItems(validSubModels);

    // 還原儲存值
    QString savedSubModel;
    if (ic.channels.size() > channelIdx && enabled &&
        validSubModels.contains(ic.channels[channelIdx].subModel))
        savedSubModel = ic.channels[channelIdx].subModel;

    cb->setCurrentText(savedSubModel);
    cb->setEnabled(enabled);

    applyComboBoxStyle(cb);

    // SubModel 改變時需要執行去重和刷新
    connect(cb, &QComboBox::currentTextChanged, this, [this](const QString&) {
        // 1. 同步到 ViewModel
        syncUIToViewModel();

        // 2. 執行 Index 唯一性檢查（關鍵！）
        enforceUniqueIndices();

        // 3. 刷新 Index 下拉選單的可用選項
        refreshIndexChoices();
    });

    return cb;
}

QComboBox* Page1::createIndexComboBox(const InstrumentConfig &ic, int channelIdx,
                                      const QString &type, bool enabled)
{
    auto *cb = new QComboBox(this);
    cb->addItem("");

    int maxOutputs = (type == "Load") ? viewModel->loadOutputs() : viewModel->relayOutputs();

    if (enabled) {
        for (int i = 1; i <= maxOutputs; ++i)
            cb->addItem(QString::number(i));
        cb->setEnabled(true);

        // 還原儲存值
        int savedIdx = (ic.channels.size() > channelIdx) ? ic.channels[channelIdx].index : 0;
        if (savedIdx > 0 && savedIdx <= maxOutputs)
            cb->setCurrentText(QString::number(savedIdx));
    } else {
        cb->setEnabled(false);
    }

    cb->installEventFilter(this);
    applyComboBoxStyle(cb);

    // Index 改變時需要執行完整的同步流程
    connect(cb, &QComboBox::currentTextChanged, this, [this](const QString&) {
        // 1. 同步到 ViewModel
        syncUIToViewModel();

        // 2. 執行 Index 唯一性檢查（關鍵！）
        enforceUniqueIndices();

        // 3. 刷新 Index 下拉選單的可用選項
        refreshIndexChoices();
    });

    return cb;
}

bool Page1::eventFilter(QObject *obj, QEvent *event)
{
    // 攔截滑鼠點擊 ComboBox 的事件
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto *cb = qobject_cast<QComboBox*>(obj)) {
            // 下拉選單即將展開,立即刷新
            refreshIndexChoices();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ==================== 事件處理 ====================

void Page1::connectModelChangeHandler(int row, const TableRowInfo &rowInfo, QComboBox *modelCb)
{
    const auto channels = viewModel->channels();

    connect(modelCb, &QComboBox::currentTextChanged, this,
            [this, row, rowInfo, channels](const QString &mName) {
                QSet<int> active = viewModel->channelsOfModel(mName);
                QStringList subList = viewModel->subModels(mName);

                for (int c = 0; c < channels.size(); ++c) {
                    int col = 3 + c;
                    bool enable = active.contains(channels[c]);

                    updateSubModelComboBox(row, col, enable, subList);
                    updateIndexComboBox(row + 1, col, enable, rowInfo.type);
                }
                enforceUniqueIndices();
            });
}

void Page1::updateSubModelComboBox(int row, int col, bool enable, const QStringList &subList)
{
    auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, col));
    if (!cb) return;

    // cb->blockSignals(true);
    cb->clear();
    cb->addItem("");

    if (enable)
        cb->addItems(subList);

    cb->setEnabled(enable);
    // cb->blockSignals(false);
    applyComboBoxStyle(cb);
}

void Page1::updateIndexComboBox(int row, int col, bool enable, const QString &type)
{
    auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, col));
    if (!cb) return;

    QString cur = cb->currentText();
    int maxOutputs = (type == "Load") ? viewModel->loadOutputs() : viewModel->relayOutputs();

    // cb->blockSignals(true);
    cb->clear();
    cb->addItem("");

    if (enable) {
        for (int i = 1; i <= maxOutputs; ++i)
            cb->addItem(QString::number(i));

        if (cur.toInt() > 0 && cur.toInt() <= maxOutputs)
            cb->setCurrentText(cur);

        cb->setEnabled(true);
    } else {
        cb->setEnabled(false);
    }

    // cb->blockSignals(false);
    applyComboBoxStyle(cb);
}

void Page1::enforceUniqueIndices()
{
    QSet<QString> loadUsed, relayUsed;

    //迴圈只處理 Index 行（奇數行）：r = 1, 3, 5, 7, 9, 11, 13 ...
    for (int r = 1; r < tableWidget->rowCount(); r += 2) {
        // 判斷這一行是 Load 還是 Relay
        auto *nameItem = tableWidget->item(r-1, 0);  // 上一行的名稱
        if (!nameItem) continue;

        QString instType = m_instrumentTypeCache.value(nameItem->text());
        if (instType.isEmpty()) continue;

        // nameItem->text() = "Load1"
        // instType = "Load"

        // nameItem->text() = "Relay1"
        // instType = "Relay"

        QSet<QString> *usedSet = (instType == "Load") ? &loadUsed : &relayUsed;

        for (int c = 3; c < tableWidget->columnCount(); ++c) { // 從col=3開始 CH開始
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(r, c)); //取得該儲存格的 ComboBox
            if (!cb) continue;
            QString v = cb->currentText();
            if (v.isEmpty()) continue;
            if (usedSet->contains(v)) cb->setCurrentText("");
            else usedSet->insert(v);
        }
    }
}

void Page1::applyComboBoxStyle(QComboBox *comboBox, bool editable)
{
    if (!comboBox) return;

    comboBox->setEditable(editable);
    comboBox->setStyleSheet(getComboBoxStyle());

    if (auto *m = qobject_cast<QStandardItemModel*>(comboBox->model())) {
        for (int i = 0; i < m->rowCount(); ++i)
            m->setData(m->index(i,0), Qt::AlignCenter, Qt::TextAlignmentRole);
    }
}

QString Page1::getComboBoxStyle() const
{
    return R"(
        QComboBox:disabled { color: gray; background-color: white; }
        QComboBox { background-color: white; border: none; }
        QComboBox::drop-down { width: 0px; border: none; }
        QComboBox::down-arrow { width: 0px; height: 0px; }
    )";
}

void Page1::updateInstrumentVisibility(const QString &inst, bool visible)
{
    bool isLoad = viewModel->hasChannelInterface(inst);

    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *item = tableWidget->item(r, 0);
        if (!item || item->text() != inst) continue;

        if (isLoad) {

            int rowTop    = r;
            int rowBottom = r + 1;
            tableWidget->setRowHidden(rowTop,    !visible);
            tableWidget->setRowHidden(rowBottom, !visible);

            int span = visible ? 2 : 1;
            for (int col = 0; col < 3; ++col)
                tableWidget->setSpan(rowTop, col, span, 1);
        }
        else {
            tableWidget->setRowHidden(r, !visible);
        }
        break;
    }
}

void Page1::onInstrumentToggled(int state)
{
    auto *cb = qobject_cast<QCheckBox*>(sender());
    if (!cb) return;

    updateInstrumentVisibility(cb->text(), state == Qt::Checked);

    enforceUniqueIndices();
    refreshIndexChoices();
    syncUIToViewModel();
}

// ==================== UI 同步函式優化實現 ====================

void Page1::syncUIToViewModel()
{
    qDebug() << "syncUIToViewModel()";
    QList<InstrumentConfig> configs = collectAllInstrumentConfigs();
    // void Page1::syncUIToViewModel() emit uiConfigChanged 後記憶體洩漏，查找後續相關signal slot
    // 頻繁操作submodel、address 會崩潰
    emit uiConfigChanged(configs, spinBox_Load_Outputs->value(), spinBox_Relay_Outputs->value());
}

// 收集所有儀器配置
QList<InstrumentConfig> Page1::collectAllInstrumentConfigs() const
{
    QMap<QString, TableRowInfo> rowInfoMap;
    for (const auto &r : viewModel->rows())
        rowInfoMap[r.instrument] = r;

    QList<InstrumentConfig> configs;

    for (int row = 0; row < tableWidget->rowCount(); ) {
        QTableWidgetItem *nameItem = tableWidget->item(row, 0);
        if (!nameItem) {
            ++row;
            continue;
        }

        QString instName = nameItem->text();
        if (!rowInfoMap.contains(instName)) {
            ++row;
            continue;
        }

        InstrumentConfig config = collectSingleInstrumentConfig(row, instName, rowInfoMap[instName]);
        configs.append(config);
    }

    return configs;
}

// 收集單個儀器配置
InstrumentConfig Page1::collectSingleInstrumentConfig(int &row, const QString &instName,
                                                      const TableRowInfo &info) const
{
    InstrumentConfig ic;
    ic.name = instName;
    ic.type = info.type;

    // 獲取 enabled 狀態
    ic.enabled = instrumentCheckboxes.value(instName, nullptr) &&
                 instrumentCheckboxes[instName]->isChecked();

    // 獲取 Model 和 Address
    QComboBox *modelCb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, 1));
    QComboBox *addrCb  = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, 2));
    ic.modelName = modelCb ? modelCb->currentText() : "";
    ic.address   = addrCb  ? addrCb->currentText()  : "";

    // 收集 Channel 設置
    if (info.hasChannels) {
        ic.channels = collectChannelSettings(row);
        row += 2;
    } else {
        row += 1;
    }

    return ic;
}

// 收集 Channel 設置
QList<ChannelSetting> Page1::collectChannelSettings(int topRow) const
{
    QList<ChannelSetting> channels;
    int botRow = topRow + 1;

    for (int c = 3; c < tableWidget->columnCount(); ++c) {
        ChannelSetting ch;

        QComboBox *subModelCb = qobject_cast<QComboBox*>(tableWidget->cellWidget(topRow, c));
        QComboBox *idxCb      = qobject_cast<QComboBox*>(tableWidget->cellWidget(botRow, c));

        ch.subModel = subModelCb ? subModelCb->currentText() : "";
        ch.index    = idxCb ? idxCb->currentText().toInt() : 0;

        channels.append(ch);
    }

    return channels;
}

void Page1::resetUIFromViewModel()
{
    QMap<QString, bool> enabledMap;
    for (const auto &ic : viewModel->currentConfig().instruments)
        enabledMap[ic.name] = ic.enabled;

    setupCheckboxes(enabledMap);

    spinBox_Load_Outputs->blockSignals(true);
    spinBox_Load_Outputs->setValue(viewModel->loadOutputs());
    spinBox_Load_Outputs->blockSignals(false);

    spinBox_Relay_Outputs->blockSignals(true);
    spinBox_Relay_Outputs->setValue(viewModel->relayOutputs());
    spinBox_Relay_Outputs->blockSignals(false);

    setupTable();

    for (auto it = instrumentCheckboxes.cbegin(); it != instrumentCheckboxes.cend(); ++it)
        updateInstrumentVisibility(it.key(), it.value()->isChecked());

    syncUIToViewModel();
}

void Page1::autoSyncOnWidgetChanged(QWidget* widget)
{
    if (auto cb = qobject_cast<QComboBox*>(widget)) {
        connect(cb, &QComboBox::currentTextChanged, this, [this](const QString&) {
            syncUIToViewModel();
        });
    }
}

// 新增私有輔助函式
void Page1::applyOutputsChange(const QString &type, int newVal)
{
    for (int r = 1; r < tableWidget->rowCount(); r += 2) {
        auto *nameItem = tableWidget->item(r-1, 0);
        if (!nameItem) continue;

        QString instType = m_instrumentTypeCache.value(nameItem->text());
        if (instType != type) continue;

        for (int c = 3; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(r, c));
            if (!cb) continue;

            QString cur = cb->currentText();
            cb->blockSignals(true);
            cb->clear();
            cb->addItem("");

            for (int i = 1; i <= newVal; ++i)
                cb->addItem(QString::number(i));

            if (!cur.isEmpty() && cur.toInt() <= newVal)
                cb->setCurrentText(cur);

            cb->blockSignals(false);
            applyComboBoxStyle(cb);
        }
    }
    enforceUniqueIndices();
    refreshIndexChoices();
}

void Page1::applyLoadOutputs(int newVal) {
    applyOutputsChange("Load", newVal);
}

void Page1::applyRelayOutputs(int newVal) {
    applyOutputsChange("Relay", newVal);
}

// ==================== Index 管理函式實現 ====================

void Page1::refreshIndexChoices()
{
    auto [loadUsed, relayUsed] = collectUsedIndices();
    updateAllComboBoxOptions(loadUsed, relayUsed);
}

// 收集所有已使用的 index
QPair<QSet<QString>, QSet<QString>> Page1::collectUsedIndices() const
{
    QSet<QString> loadUsed, relayUsed;

    for (int r = 1; r < tableWidget->rowCount(); r += 2) {
        QString instType = getInstrumentType(r - 1);
        if (instType.isEmpty()) continue;

        QSet<QString> &usedSet = (instType == "Load") ? loadUsed : relayUsed;

        // 收集該行所有已使用的 index
        for (int c = 3; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(r, c));
            if (cb && !cb->currentText().isEmpty()) {
                usedSet.insert(cb->currentText());
            }
        }
    }

    return {loadUsed, relayUsed};
}

// 更新所有 ComboBox 的可用選項
void Page1::updateAllComboBoxOptions(const QSet<QString> &loadUsed,
                                     const QSet<QString> &relayUsed)
{
    for (int r = 1; r < tableWidget->rowCount(); r += 2) {
        QString instType = getInstrumentType(r - 1);
        if (instType.isEmpty()) continue;

        const QSet<QString> &usedSet = (instType == "Load") ? loadUsed : relayUsed;

        // 更新該行所有 ComboBox
        for (int c = 3; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(r, c));
            if (cb) {
                updateComboBoxModel(cb, usedSet);
            }
        }
    }
}

// 更新單個 ComboBox 的模型
void Page1::updateComboBoxModel(QComboBox *cb, const QSet<QString> &usedSet) const
{
    auto *model = qobject_cast<QStandardItemModel*>(cb->model());
    if (!model) return;

    QString currentText = cb->currentText();

    for (int row = 0; row < model->rowCount(); ++row) {
        QString itemText = model->data(model->index(row, 0)).toString();
        bool shouldEnable = isItemAvailable(itemText, currentText, usedSet);
        model->item(row)->setEnabled(shouldEnable);
    }
}

// 獲取儀器類型 (Load/Relay)
QString Page1::getInstrumentType(int nameRow) const
{
    auto *nameItem = tableWidget->item(nameRow, 0); // 取得單一儲存格QTableWidgetItem nameItem
    if (!nameItem) return QString();

    // for (auto it = m_instrumentTypeCache.cbegin(); it != m_instrumentTypeCache.cend(); ++it) {
    //     qDebug() << "Instrument:" << it.key() << "Type:" << it.value();}

    // Instrument: "Load1" Type: "Load"
    // Instrument: "Load2" Type: "Load"
    // Instrument: "Load3" Type: "Load"
    // Instrument: "Load4" Type: "Load"
    // Instrument: "Load5" Type: "Load"
    // Instrument: "Oscilloscope" Type: "Oscilloscope"
    // Instrument: "Relay" Type: "Relay"
    // Instrument: "Relay2" Type: "Relay"
    // Instrument: "Source" Type: "InputSource"

    // m_instrumentTypeCache = {
    //     {"Load1",  "Load"},
    //     {"Load2",  "Load"},
    //     {"Load3",  "Load"},
    //     {"Load4",  "Load"},
    //     {"Load5",  "Load"},
    //     {"Oscilloscope","Oscilloscope"}
    //     {"Relay1", "Relay"},
    //     {"Relay2", "Relay"},
    //     {"Source", "InputSource"}
    // };

    return m_instrumentTypeCache.value(nameItem->text()); // nameItem->text() = Load1、Load2.......Relay2
}

// 判斷選項是否可用
bool Page1::isItemAvailable(const QString &itemText,
                            const QString &currentText,
                            const QSet<QString> &usedSet) const
{
    return itemText.isEmpty()
    || !usedSet.contains(itemText)
        || itemText == currentText;
}
