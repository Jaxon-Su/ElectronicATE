#include "page1.h"
#include "outputindexpolicy.h"
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
#include "communicationconfigdialog.h"

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
    tableWidget = new QTableWidget(this);
    tableWidget->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { border: none; border-bottom: 1px solid #e0e0e0; }"
        );

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

    groupBox->setStyleSheet(R"(
        QGroupBox { margin-top:0px; padding-top:4px; border:1px solid lightgray; }
        QGroupBox::title { subcontrol-origin:content; subcontrol-position:top left; padding:2px 5px; }
    )");

    label1 = new QLabel("  Load Outputs", this);
    QFont boldFont = label1->font();
    boldFont.setBold(true);
    label1->setFont(boldFont);

    label2 = new QLabel("  Relay Outputs", this);
    label2->setFont(boldFont);

    loadSpinWidget = new QWidget(this);
    relaySpinWidget = new QWidget(this);
    leftContainer = new QWidget(this);

}

void Page1::setupLayout()
{
    checkboxLayout = new QVBoxLayout(groupBox);
    checkboxLayout->setSpacing(5);
    checkboxLayout->setContentsMargins(5, 5, 5, 5);

    // Load Output + SpinBox 一組
    QVBoxLayout *loadSpinLayout = new QVBoxLayout;
    loadSpinLayout->setSpacing(2);
    loadSpinLayout->setContentsMargins(0, 0, 0, 0);
    loadSpinLayout->addWidget(label1);
    loadSpinLayout->addWidget(spinBox_Load_Outputs);

    // QWidget *loadSpinWidget = new QWidget(this);
    loadSpinWidget->setLayout(loadSpinLayout);
    loadSpinWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Relay Output + SpinBox 一組
    QVBoxLayout *relaySpinLayout = new QVBoxLayout;
    relaySpinLayout->setSpacing(2);
    relaySpinLayout->setContentsMargins(0, 0, 0, 0);
    relaySpinLayout->addWidget(label2);
    relaySpinLayout->addWidget(spinBox_Relay_Outputs);

    // QWidget *relaySpinWidget = new QWidget(this);
    relaySpinWidget->setLayout(relaySpinLayout);
    relaySpinWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // QWidget *leftContainer = new QWidget(this);
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

void Page1::setupConnections()
{
    connect(spinBox_Load_Outputs, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Page1::syncUIToViewModel);

    connect(spinBox_Relay_Outputs, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Page1::syncUIToViewModel);

    connect(viewModel, &Page1ViewModel::loadOutputsChanged,
            this,      &Page1::applyLoadOutputs);

    connect(viewModel, &Page1ViewModel::relayOutputsChanged,
            this,      &Page1::applyRelayOutputs);

    connect(viewModel, &Page1ViewModel::dataChanged,
            this,      &Page1::resetUIFromViewModel);

    connect(this, &Page1::uiConfigChanged, viewModel, &Page1ViewModel::onUiConfigChanged);

}

// ==================== 輔助函式 ====================

void Page1::prepareTableStructure()
{
    const auto channels = viewModel->channels();
    const int nCols = 4 + channels.size();

    int totalRows = 0;
    for (const auto &r : viewModel->rows())
        totalRows += r.hasChannels ? 3 : 1;

    tableWidget->clear();
    tableWidget->setColumnCount(nCols);
    tableWidget->setRowCount(totalRows);

    // 設定表頭
    QStringList hdr{ "Instrument", "Model", "Address", "" };
    for (int ch : channels)
        hdr << QString("Ch%1").arg(ch);

    tableWidget->setHorizontalHeaderLabels(hdr);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->verticalHeader()->setDefaultSectionSize(22);
    tableWidget->verticalHeader()->setMinimumSectionSize(18);

    // ===== 修改列寬設定 =====
    // 先設為 Interactive 讓用戶可手動調整
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // 設定各列初始寬度
    tableWidget->setColumnWidth(0, 85);    // Instrument
    tableWidget->setColumnWidth(1, 105);   // Model
    tableWidget->setColumnWidth(2, 115);   // Address
    tableWidget->setColumnWidth(3, 31);    // 配置按鈕

    // Channel 欄固定寬度 + 水平捲動條（channels 多時不壓縮）
    static constexpr int kChColWidth = 100;
    for (int c = 4; c < nCols; ++c) {
        tableWidget->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
        tableWidget->setColumnWidth(c, kChColWidth);
    }

    // 配置按鈕欄固定寬度
    tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);

    // 水平捲動條（欄數超出視窗時自動出現）
    tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void Page1::buildConfigMap()
{
    m_configMap.clear();
    m_instrumentWidgets.clear();
    m_instrumentTypeCache.clear();
    m_commConfigMap.clear();

    for (const auto &ic : viewModel->currentConfig().instruments) {
        m_configMap[ic.name] = ic;
        m_commConfigMap[ic.name] = ic.commConfig;
    }

    // 快速查詢儀器 type
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

    // Model ComboBox (欄位 1)
    widgets.modelCb = createModelComboBox(rowInfo.modelCandidates, ic.modelName);
    tableWidget->setCellWidget(row, 1, widgets.modelCb);

    // Address ComboBox (欄位 2) - 修改為傳入完整配置
    widgets.addrCb = createAddressComboBox(ic);
    tableWidget->setCellWidget(row, 2, widgets.addrCb);

    // Config Button (欄位 3)
    widgets.configBtn = createConfigButton(rowInfo.instrument);
    tableWidget->setCellWidget(row, 3, widgets.configBtn);

    // 儲存通訊配置
    m_commConfigMap[rowInfo.instrument] = ic.commConfig;

    m_instrumentWidgets[rowInfo.instrument] = widgets;
}

int Page1::setupChannelRow(int startRow, const TableRowInfo &rowInfo, const InstrumentConfig &ic)
{
    tableWidget->setSpan(startRow, 0, 3, 1);  // Instrument
    tableWidget->setSpan(startRow, 1, 3, 1);  // Model
    tableWidget->setSpan(startRow, 2, 3, 1);  // Address
    tableWidget->setSpan(startRow, 3, 3, 1);  // Config Button

    QSet<int> validChannels = viewModel->channelsOfModel(ic.modelName);
    QStringList validSubModels = viewModel->subModels(ic.modelName);
    const auto channels = viewModel->channels();

    RowWidgets &widgets = m_instrumentWidgets[rowInfo.instrument];

    for (int c = 0; c < channels.size(); ++c) {
        int col = 4 + c;
        bool chEnabled = validChannels.contains(channels[c]);

        // 上排 subModel
        auto *subModelCb = createSubModelComboBox(ic, c, chEnabled, validSubModels);
        widgets.subModelCbs.append(subModelCb);
        tableWidget->setCellWidget(startRow, col, subModelCb);

        // 下排 index
        auto *indexCb = createIndexComboBox(ic, c, rowInfo.type, chEnabled);
        widgets.indexCbs.append(indexCb);
        tableWidget->setCellWidget(startRow + 1, col, indexCb);

        auto *syncRoleCb = createSyncRoleComboBox(ic, c, rowInfo.type, chEnabled);
        widgets.syncRoleCbs.append(syncRoleCb);
        tableWidget->setCellWidget(startRow + 2, col, syncRoleCb);
    }

    // 連接 model 變更事件
    connectModelChangeHandler(startRow, rowInfo, widgets.modelCb);

    return startRow + 3;
}

int Page1::setupSimpleRow(int startRow)
{
    const int nCols = tableWidget->columnCount();
    for (int c = 4; c < nCols; ++c) {  // 修改：從 4 開始（原本是 3）
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

QComboBox* Page1::createAddressComboBox(const InstrumentConfig &ic)
{
    auto *cb = new QComboBox(this);
    cb->setEditable(true);

    // 添加常用 GPIB 地址選項
    cb->addItem("");  // 空選項
    for (int i = 1; i <= 30; ++i)
        cb->addItem(QString::number(i));

    // 添加常用的 VXI-11 TCP 格式提示
    cb->addItem("TCPIP0::192.168.0.6::INSTR");
    cb->addItem("TCPIP0::192.168.1.100::INSTR");

    // 設置文字對齊
    if (cb->lineEdit())
        cb->lineEdit()->setAlignment(Qt::AlignCenter);

    applyComboBoxStyle(cb, true);

    // 設定顯示值
    QString displayAddr;
    if (ic.commConfig.isValid()) {
        displayAddr = ic.commConfig.toResourceString();
    } else if (!ic.address.isEmpty()) {
        displayAddr = ic.address;
    }

    if (!displayAddr.isEmpty()) {
        int index = cb->findText(displayAddr);
        if (index >= 0) {
            cb->setCurrentIndex(index);
        } else {
            cb->addItem(displayAddr);
            cb->setCurrentText(displayAddr);
        }
    } else {
        cb->setCurrentIndex(0);
    }

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

        // 2. 執行 Index 唯一性檢查
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
        cb->addItems(OutputIndexPolicy::choices(maxOutputs).mid(1));
        cb->setEnabled(true);

        // 還原儲存值
        int savedIdx = (ic.channels.size() > channelIdx) ? ic.channels[channelIdx].index : 0;
        if (OutputIndexPolicy::isValid(savedIdx, maxOutputs))
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

        // 2. 執行 Index 唯一性檢查
        enforceUniqueIndices();

        // 3. 刷新 Index 下拉選單的可用選項
        refreshIndexChoices();
    });

    return cb;
}

QComboBox* Page1::createSyncRoleComboBox(const InstrumentConfig &ic, int channelIdx,
                                         const QString &type, bool enabled)
{
    auto *cb = new QComboBox(this);
    cb->addItem("");
    cb->addItems({"MASTER", "SLAVE", "NONE"});

    const bool usable = enabled && type == "Load";
    cb->setEnabled(usable);

    int savedType = (ic.channels.size() > channelIdx) ? ic.channels[channelIdx].syncType : -1;
    if (usable) {
        if (savedType == 1)
            cb->setCurrentText("MASTER");
        else if (savedType == 2)
            cb->setCurrentText("SLAVE");
        else if (savedType == 0)
            cb->setCurrentText("NONE");
    }

    applyComboBoxStyle(cb);
    connect(cb, &QComboBox::currentTextChanged, this, [this](const QString&) {
        syncUIToViewModel();
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
                    int col = 4 + c;
                    bool enable = active.contains(channels[c]);

                    updateSubModelComboBox(row, col, enable, subList);
                    updateIndexComboBox(row + 1, col, enable, rowInfo.type);
                    updateSyncRoleComboBox(row + 2, col, enable, rowInfo.type);
                }
                enforceUniqueIndices();
            });
}

void Page1::updateSyncRoleComboBox(int row, int col, bool enable, const QString &type)
{
    auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, col));
    if (!cb) return;

    const QString cur = cb->currentText();
    cb->clear();
    cb->addItem("");
    cb->addItems({"MASTER", "SLAVE", "NONE"});
    cb->setCurrentText(cur);
    cb->setEnabled(enable && type == "Load");
    applyComboBoxStyle(cb);
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
        cb->addItems(OutputIndexPolicy::choices(maxOutputs).mid(1));

        if (!OutputIndexPolicy::restoredChoice(cur, maxOutputs).isEmpty())
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

    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *nameItem = tableWidget->item(r, 0);
        if (!nameItem) continue;

        QString instType = m_instrumentTypeCache.value(nameItem->text());
        if (instType.isEmpty()) continue;

        QSet<QString> *usedSet = (instType == "Load") ? &loadUsed : &relayUsed;
        const int indexRow = viewModel->hasChannelInterface(nameItem->text()) ? r + 1 : r;

        for (int c = 4; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(indexRow, c));
            if (!cb) continue;
            QString v = cb->currentText();
            if (v.isEmpty()) continue;
            if (!OutputIndexPolicy::claim(v, *usedSet)) cb->setCurrentText("");
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
        QComboBox { background-color: white; border: none; min-height: 18px; padding: 0px; }
        QComboBox::drop-down { width: 0px; border: none; }
        QComboBox::down-arrow { width: 0px; height: 0px; }
    )";
}

void Page1::updateInstrumentVisibility(const QString &inst, bool visible)
{
    bool isLoadRelay = viewModel->hasChannelInterface(inst);

    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *item = tableWidget->item(r, 0);
        if (!item || item->text() != inst) continue;

        if (isLoadRelay) {
            int rowTop    = r;
            int rowBottom = r + 1;
            int rowSync   = r + 2;
            tableWidget->setRowHidden(rowTop,    !visible);
            tableWidget->setRowHidden(rowBottom, !visible);
            tableWidget->setRowHidden(rowSync,   !visible);

            int span = visible ? 3 : 1;
            for (int col = 0; col < 4; ++col)  // 修改：前四欄都要 span（原本是 3）
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
    // qDebug() << "syncUIToViewModel()";
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

    // 獲取 Model
    QComboBox *modelCb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, 1));
    ic.modelName = modelCb ? modelCb->currentText() : "";

    // 獲取 Address
    QComboBox *addrCb = qobject_cast<QComboBox*>(tableWidget->cellWidget(row, 2));
    QString addrText = addrCb ? addrCb->currentText() : "";

    // 優先使用儲存的 CommunicationConfig
    if (m_commConfigMap.contains(instName) && m_commConfigMap[instName].isValid()) {
        ic.commConfig = m_commConfigMap[instName];
        ic.address = ic.commConfig.toResourceString();
    } else {
        // 嘗試從文字解析
        ic.setAddress(addrText);
    }

    // 收集 Channel 設置
    if (info.hasChannels) {
        ic.channels = collectChannelSettings(row);
        row += 3;
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
    int syncRow = topRow + 2;

    for (int c = 4; c < tableWidget->columnCount(); ++c) {
        ChannelSetting ch;

        QComboBox *subModelCb = qobject_cast<QComboBox*>(tableWidget->cellWidget(topRow, c));
        QComboBox *idxCb      = qobject_cast<QComboBox*>(tableWidget->cellWidget(botRow, c));
        QComboBox *syncCb     = qobject_cast<QComboBox*>(tableWidget->cellWidget(syncRow, c));

        ch.subModel = subModelCb ? subModelCb->currentText() : "";
        ch.index    = idxCb ? idxCb->currentText().toInt() : 0;
        const QString syncText = syncCb ? syncCb->currentText().trimmed().toUpper() : "";
        if (syncText == "NONE")
            ch.syncType = 0;
        else if (syncText == "MASTER")
            ch.syncType = 1;
        else if (syncText == "SLAVE")
            ch.syncType = 2;

        channels.append(ch);
    }

    return channels;
}

void Page1::resetUIFromViewModel()
{
    m_commConfigMap.clear();
    for (const auto &ic : viewModel->currentConfig().instruments) {
        if (ic.commConfig.isValid()) {
            m_commConfigMap[ic.name] = ic.commConfig;
        }
    }

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

void Page1::applyOutputsChange(const QString &type, int newVal)
{
    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *nameItem = tableWidget->item(r, 0);
        if (!nameItem) continue;

        QString instType = m_instrumentTypeCache.value(nameItem->text());
        if (instType != type) continue;
        if (!viewModel->hasChannelInterface(nameItem->text())) continue;

        const int indexRow = r + 1;
        for (int c = 4; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(indexRow, c));
            if (!cb) continue;

            QString cur = cb->currentText();
            cb->blockSignals(true);
            cb->clear();
            cb->addItem("");

            cb->addItems(OutputIndexPolicy::choices(newVal).mid(1));

            if (!OutputIndexPolicy::restoredChoice(cur, newVal).isEmpty())
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

    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *nameItem = tableWidget->item(r, 0);
        if (!nameItem || !viewModel->hasChannelInterface(nameItem->text())) continue;

        QString instType = getInstrumentType(r);
        if (instType.isEmpty()) continue;

        QSet<QString> &usedSet = (instType == "Load") ? loadUsed : relayUsed;
        const int indexRow = r + 1;

        // 收集該行所有已使用的 index
        for (int c = 4; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(indexRow, c));
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
    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto *nameItem = tableWidget->item(r, 0);
        if (!nameItem || !viewModel->hasChannelInterface(nameItem->text())) continue;

        QString instType = getInstrumentType(r);
        if (instType.isEmpty()) continue;

        const QSet<QString> &usedSet = (instType == "Load") ? loadUsed : relayUsed;
        const int indexRow = r + 1;

        // 更新該行所有 ComboBox
        for (int c = 4; c < tableWidget->columnCount(); ++c) {
            auto *cb = qobject_cast<QComboBox*>(tableWidget->cellWidget(indexRow, c));
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
    return OutputIndexPolicy::isAvailable(itemText, currentText, usedSet);
}

QPushButton* Page1::createConfigButton(const QString &instName)
{
    auto *btn = new QPushButton("⚙", this);
    btn->setFixedSize(31, 25);
    btn->setToolTip(tr("開啟通訊配置對話框"));
    btn->setProperty("instrumentName", instName);

    // 設置按鈕樣式
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 3px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
    )");

    connect(btn, &QPushButton::clicked, this, &Page1::onConfigButtonClicked);

    return btn;
}

void Page1::onConfigButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    QString instName = btn->property("instrumentName").toString();
    openCommConfigDialog(instName);
}

void Page1::openCommConfigDialog(const QString &instName)
{
    CommunicationConfigDialog dlg(this);
    dlg.setWindowTitle(tr("通訊設定 - %1").arg(instName));

    // 設定當前配置
    if (m_commConfigMap.contains(instName) && m_commConfigMap[instName].isValid()) {
        dlg.setConfig(m_commConfigMap[instName]);
    } else {
        if (m_instrumentWidgets.contains(instName)) {
            QComboBox *addrCb = m_instrumentWidgets[instName].addrCb;
            if (addrCb) {
                QString addr = addrCb->currentText();
                if (!addr.isEmpty()) {
                    CommunicationConfig cfg = CommunicationConfig::fromResourceString(addr);
                    dlg.setConfig(cfg);
                }
            }
        }
    }

    if (dlg.exec() == QDialog::Accepted) {
        CommunicationConfig newConfig = dlg.getConfig();
        m_commConfigMap[instName] = newConfig;

        // 更新 Address ComboBox 顯示
        if (m_instrumentWidgets.contains(instName)) {
            QComboBox *addrCb = m_instrumentWidgets[instName].addrCb;
            if (addrCb) {
                QString resourceStr = newConfig.toResourceString();

                addrCb->blockSignals(true);
                int idx = addrCb->findText(resourceStr);
                if (idx < 0) {
                    addrCb->addItem(resourceStr);
                    idx = addrCb->count() - 1;
                }
                addrCb->setCurrentIndex(idx);
                if (addrCb->lineEdit()) {
                    addrCb->lineEdit()->setText(resourceStr);
                }
                addrCb->blockSignals(false);
            }
        }

        syncUIToViewModel();
    }
}
