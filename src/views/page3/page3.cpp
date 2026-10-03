#include "page3.h"
#include "triggermodelcatalog.h"
#include <QFileDialog>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QHeaderView>
#include <QTimer>
#include <QStyle>
#include "styleutils.h"

namespace {
void applyOutputStatusStyle(QPushButton* button, bool busy, bool unknown)
{
    const QString status = busy ? QStringLiteral("busy") :
                           unknown ? QStringLiteral("unknown") : QStringLiteral("normal");
    if (button->property("outputStatus").toString() == status)
        return;
    button->setProperty("outputStatus", status);
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}
} // namespace

// 建構函數
Page3::Page3(Page3ViewModel* viewModel, QWidget* parent) : QWidget(parent), vm(viewModel)
{
    if (vm) {
        vm->setCaptureFileSelector([page = QPointer<Page3>(this)](const QString& title,
                                                                  const QString& defaultPath,
                                                                  const QString& filter) {
            if (!page)
                return QString{};
            return QFileDialog::getSaveFileName(page.data(), title, defaultPath, filter);
        });
    }
    initializeUI();
    buildLayout();
    applyStyles();
    setupConnections();
    updateScopeControls();
}

// 初始化

void Page3::initializeUI()
{
    // Input Group
    btnInput = createPushButton(tr("ON"));
    btnChange = createPushButton(tr("change"), "btnChange");
    btnInput->setCheckable(true);
    btnInput->setFixedSize(kBtnWidth, kBtnHeight);
    btnChange->setFixedSize(kBtnWidth, kBtnHeight);

    cmbInput = new QComboBox(this);
    cmbInput->setEditable(false);
    cmbInput->setMinimumWidth(kBtnWidth);

    grpInput = new QGroupBox(tr("AC Input"));
    grpInput->setFont(QFont(font().family(), 9, QFont::Bold));
    grpInput->setFixedWidth(200);

    {
        const QString suffix = "1";
        grpDcInput = new QGroupBox(tr("DC Input"));
        grpDcInput->setObjectName("grpDcInput" + suffix);
        grpDcInput->setFont(grpInput->font());
        grpDcInput->setFixedWidth(200);
        cmbDcInput = new QComboBox(this);
        cmbDcInput->setObjectName("cmbDcInput" + suffix);
        cmbDcInput->setEditable(false);
        cmbDcInput->setMinimumWidth(kBtnWidth);
        cmbDcTarget = new QComboBox(this);
        cmbDcTarget->setObjectName("cmbDcTarget");
        cmbDcTarget->addItem(tr("Group: all enabled DC"), -1);
        for (int source = 0; source < 3; ++source)
            cmbDcTarget->addItem(tr("Single: DC%1").arg(source + 1), source);
        lblDcState = new QLabel(this);
        lblDcState->setObjectName("lblDcState");
        btnDcInput = createPushButton(tr("ON"), "btnDcInput" + suffix);
        btnDcInput->setCheckable(true);
        btnDcChange = createPushButton(tr("change"), "btnDcChange" + suffix);
        for (auto* button : {btnDcInput, btnDcChange}) {
            button->setFixedSize(kBtnWidth, kBtnHeight);
        }
    }

    // Load Group
    btnLoadOn = createPushButton(tr("ON"));
    btnLoadChg = createPushButton(tr("change"), "btnLoadChg");
    btnLoadOn->setCheckable(true);
    btnLoadOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnLoadChg->setFixedSize(kBtnWidth, kBtnHeight);

    cmbLoad = new QComboBox(this);
    cmbLoad->setEditable(false);
    cmbLoad->setMinimumWidth(kBtnWidth);

    grpLoad = new QGroupBox(tr("Load"));
    grpLoad->setFont(QFont(font().family(), 9, QFont::Bold));
    grpLoad->setFixedWidth(200);

    // Dynamic Load Group
    btnDyloadOn = createPushButton(tr("ON"));
    btnDyloadChg = createPushButton(tr("change"), "btnLoadChg");
    btnDyloadOn->setCheckable(true);
    btnDyloadOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnDyloadChg->setFixedSize(kBtnWidth, kBtnHeight);

    // chkDyload = new QCheckBox(tr("Synchronous"), this);
    // chkDyload->setObjectName("chkDyload");

    cmbDyload = new QComboBox(this);
    cmbDyload->setEditable(false);
    cmbDyload->setMinimumWidth(kBtnWidth);

    grpDyload = new QGroupBox(tr("Dynamic Load"));
    grpDyload->setFont(QFont(font().family(), 9, QFont::Bold));
    grpDyload->setFixedWidth(200);

    // Relay Group
    btnRelayOn = createPushButton(tr("ON"));
    btnRelayChg = createPushButton(tr("change"), "btnRelayChg");
    btnRelayOn->setCheckable(true);
    btnRelayOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnRelayChg->setFixedSize(kBtnWidth, kBtnHeight);

    cmbRelay = new QComboBox(this);
    cmbRelay->setEditable(false);
    cmbRelay->setMinimumWidth(kBtnWidth);

    grpRelay = new QGroupBox(tr("Relay"));
    grpRelay->setFont(QFont(font().family(), 9, QFont::Bold));
    grpRelay->setFixedWidth(200);

    // Capture Group
    cmbCaptureChannel = new QComboBox(this);
    cmbCaptureChannel->setObjectName("cmbCaptureChannel");
    cmbCaptureChannel->setEnabled(false);
    cmbCaptureChannel->setToolTip(tr("Channel for CSV / WFM capture."));
    btnPic = createPushButton(tr("PNG"), "btnPic");
    btnCsv = createPushButton(tr("CSV"), "btnCsv");
    btnAllCsv = createPushButton("AllCSV", "btnAllCsv");
    btnWfm = createPushButton(tr("WFM"), "btnWfm");
    btnAllWfm = createPushButton(tr("AllWFM"), "btnAllWfm");
    btnPic->setToolTip(tr("Save the oscilloscope screen."));
    btnCsv->setToolTip(tr("Save CSV for the Capture channel."));
    btnWfm->setToolTip(tr("Save WFM for the Capture channel."));
    btnAllCsv->setToolTip(tr("Save CSV for all enabled channels."));
    btnAllWfm->setToolTip(tr("Save WFM for all enabled channels."));

    //btnAllCsv
    btnPic->setFixedSize(kBtnWidth, kBtnHeight);
    btnCsv->setFixedSize(kBtnWidth, kBtnHeight);
    btnWfm->setFixedSize(kBtnWidth, kBtnHeight);
    btnAllWfm->setFixedSize(kBtnWidth, kBtnHeight);
    btnAllCsv->setFixedSize(kBtnWidth, kBtnHeight);

    grpCap = new QGroupBox(tr("Capture"));
    grpCap->setObjectName("grpCapture");
    grpCap->setFont(QFont(font().family(), 9, QFont::Bold));

    // Table
    tblTop = new QTableWidget(this);
    tblTop->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblTop->verticalHeader()->setVisible(false);
    tblTop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 控制區
    m_ctrlArea = new QWidget(this);
}

void Page3::buildLayout()
{
    // 創建各組的佈局（使用 lambda 減少重複）
    auto createGroupLayout = [](QGroupBox* grp, QComboBox* cmb, QPushButton* btn1, QPushButton* btn2,
                                QCheckBox* chk = nullptr) {
        auto* lay = new QVBoxLayout(grp);
        lay->setSpacing(6);
        lay->setContentsMargins(4, 16, 4, 8);
        lay->addWidget(cmb);

        if (chk)
            lay->addWidget(chk);

        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        row->addWidget(btn1);
        row->addWidget(btn2);
        lay->addLayout(row);
    };

    createGroupLayout(grpInput, cmbInput, btnInput, btnChange);
    createGroupLayout(grpDcInput, cmbDcInput, btnDcInput, btnDcChange);
    auto* dcLayout = qobject_cast<QVBoxLayout*>(grpDcInput->layout());
    dcLayout->insertWidget(1, cmbDcTarget);
    dcLayout->insertWidget(2, lblDcState);
    createGroupLayout(grpLoad, cmbLoad, btnLoadOn, btnLoadChg);
    createGroupLayout(grpDyload, cmbDyload, btnDyloadOn, btnDyloadChg, chkDyload);
    createGroupLayout(grpRelay, cmbRelay, btnRelayOn, btnRelayChg);

    // Capture Group
    auto* layCap = new QGridLayout(grpCap);
    layCap->setSpacing(4);
    layCap->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layCap->setContentsMargins(4, 18, 4, 8);
    for (auto* button : {btnPic, btnCsv, btnAllCsv, btnWfm, btnAllWfm}) {
        button->setFixedSize(90, 30);
        button->setStyleSheet("QPushButton { font-size: 12px; padding: 1px 0px; min-height: 0px; min-width: "
                              "86px; max-width: 86px; }");
    }
    auto* captureChannelRow = new QHBoxLayout;
    auto* captureChannelLabel = new QLabel(tr("CH:"), grpCap);
    captureChannelLabel->setBuddy(cmbCaptureChannel);
    captureChannelRow->addWidget(captureChannelLabel);
    captureChannelRow->addWidget(cmbCaptureChannel, 1);
    layCap->addLayout(captureChannelRow, 0, 0, 1, 2);
    layCap->addWidget(btnPic, 1, 0);
    layCap->addWidget(btnCsv, 2, 0);
    layCap->addWidget(btnAllCsv, 2, 1);
    layCap->addWidget(btnWfm, 3, 0);
    layCap->addWidget(btnAllWfm, 3, 1);
    grpCap->setFixedWidth(200);

    // 左欄
    auto* leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(6, 6, 6, 6);
    leftCol->setSpacing(10);
    leftCol->addWidget(grpInput);
    leftCol->addWidget(grpLoad);
    leftCol->addWidget(grpDyload);
    leftCol->addWidget(grpRelay);
    leftCol->addWidget(grpDcInput);
    leftCol->addStretch();
    leftCol->addWidget(grpCap);

    auto* leftFrame = new QFrame;
    leftFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
    leftFrame->setLineWidth(1);
    leftFrame->setLayout(leftCol);
    leftFrame->setFixedWidth(leftFrame->sizeHint().width());
    auto* leftScroll = new QScrollArea(this);
    leftScroll->setObjectName("page3InputScroll");
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setWidgetResizable(true);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setWidget(leftFrame);
    leftScroll->setFixedWidth(leftFrame->width() + leftScroll->verticalScrollBar()->sizeHint().width());

    // 右欄
    auto* rightFrame = new QFrame;
    rightFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
    rightFrame->setLineWidth(1);

    auto* rightLay = new QVBoxLayout(rightFrame);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(4);
    rightLay->addWidget(m_ctrlArea);

    // 主佈局
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);
    root->addWidget(leftScroll);
    root->addWidget(rightFrame, 1); /*伸展因子=1（佔據剩餘空間）*/

    // 控制區
    m_ctrlLay = new QHBoxLayout(m_ctrlArea);
    m_ctrlLay->setContentsMargins(0, 0, 0, 0);
    m_ctrlLay->setSpacing(0);
}

void Page3::setupConnections()
{
    connect(vm, &Page3ViewModel::dcInputUpdated, this,
            [this](int source, const QStringList& titles, int index) {
                if (source != 0)
                    return;
                const QSignalBlocker blocker(cmbDcInput);
                cmbDcInput->clear();
                for (int i = 0; i < titles.size(); ++i)
                    if (!titles[i].trimmed().isEmpty())
                        cmbDcInput->addItem(titles[i], i);
                cmbDcInput->setCurrentIndex(cmbDcInput->findData(index));
                for (int i = 0; i < 3; ++i)
                    vm->onDcSelected(i, index, cmbDcInput->currentText());
            });
    connect(vm, &Page3ViewModel::dcOutputStateChanged, this, [this](int, bool) { updateDcControls(); });
    connect(vm, &Page3ViewModel::dcOperationBusyChanged, this,
            [this](int, bool) { updateDcControls(); });
    {
        connect(cmbDcTarget, &QComboBox::currentIndexChanged, this, [this] { updateDcControls(); });
        connect(btnDcInput, &QPushButton::toggled, this, [this](bool on) {
            const int source = cmbDcTarget->currentData().toInt();
            if (source < 0)
                vm->onDcGroupToggled(on);
            else
                vm->onDcInputToggled(source, on);
            updateDcControls();
        });
        connect(btnDcChange, &QPushButton::clicked, this, [this] {
            const int source = cmbDcTarget->currentData().toInt();
            if (source < 0)
                vm->onDcGroupChanged();
            else
                vm->onDcInputChanged(source);
        });
        connect(cmbDcInput, &QComboBox::currentIndexChanged, this, [this](int index) {
            for (int source = 0; source < 3; ++source)
                vm->onDcSelected(source, index < 0 ? -1 : cmbDcInput->currentData().toInt(),
                                 cmbDcInput->currentText());
        });
    }
    vm->refreshDcInputs();
    updateDcControls();
    // Toggle 按鈕連接（使用輔助函數減少重複）
    connectToggleButton(btnInput, &Page3::inputToggled);
    connectToggleButton(btnLoadOn, &Page3::loadToggled);
    connectToggleButton(btnDyloadOn, &Page3::dyloadToggled);
    connectToggleButton(btnRelayOn, &Page3::relayToggled);

    // ComboBox 選擇變更（使用輔助函數）
    connectComboBox(cmbInput, TableKind::Input);
    connectComboBox(cmbLoad, TableKind::Load);
    connectComboBox(cmbDyload, TableKind::DyLoad);
    connectComboBox(cmbRelay, TableKind::Relay);

    // Change 按鈕連接（使用輔助函數）
    connectChangeButton(btnChange, &Page3::inputChanged);
    connectChangeButton(btnLoadChg, &Page3::loadChanged);
    connectChangeButton(btnDyloadChg, &Page3::dyloadChanged);
    connectChangeButton(btnRelayChg, &Page3::relayChanged);
    connectChangeButton(btnPic, &Page3::waveformCaptured);
    connect(btnCsv, &QPushButton::clicked, this,
            [this] { emit csvCaptured(cmbCaptureChannel->currentData().toInt()); });
    connectChangeButton(btnAllCsv, &Page3::allcsvCaptured);
    connect(btnWfm, &QPushButton::clicked, this,
            [this] { emit wfmCaptured(cmbCaptureChannel->currentData().toInt()); });
    connectChangeButton(btnAllWfm, &Page3::allwfmCaptured);

    // Load/DyLoad 互鎖
    connect(btnLoadOn, &QPushButton::toggled, this, [this](bool) { loadLock(); });
    connect(btnDyloadOn, &QPushButton::toggled, this, [this](bool) { loadLock(); });

    // ViewModel 連接
    connect(this, &Page3::selectedChanged, vm, &Page3ViewModel::onSelected);
    connect(this, &Page3::inputToggled, vm, &Page3ViewModel::onInputToggled);
    connect(this, &Page3::inputChanged, vm, &Page3ViewModel::onInputChanged);
    connect(this, &Page3::loadToggled, vm, &Page3ViewModel::onLoadToggled);
    connect(this, &Page3::loadChanged, vm, &Page3ViewModel::onLoadChanged);
    connect(this, &Page3::dyloadToggled, vm, &Page3ViewModel::onDyloadToggled);
    connect(this, &Page3::dyloadChanged, vm, &Page3ViewModel::onDyLoadChanged);
    connect(this, &Page3::relayToggled, vm, &Page3ViewModel::onRelayToggled);
    connect(this, &Page3::relayChanged, vm, &Page3ViewModel::onRelayChanged);

    connect(vm, &Page3ViewModel::forceOff, this, &Page3::forceButtonOff);
    connect(vm, &Page3ViewModel::restoreOutputState, this, &Page3::setOutputButtonState);
    connect(vm, &Page3ViewModel::page1ConfigChanged, this, &Page3::onPage1ConfigChanged);
    connect(vm, &Page3ViewModel::headersChanged, this, &Page3::onHeadersChanged);
    connect(vm, &Page3ViewModel::rowLabelsChanged, this, &Page3::onRowLabelsChanged);
    connect(vm, &Page3ViewModel::titlesUpdated, this, &Page3::onTitlesUpdated);
    connect(vm, &Page3ViewModel::restoreSelections, this, &Page3::onRestoreSelections);
    connect(vm, &Page3ViewModel::loadOperationBusyChanged, this, &Page3::setLoadOperationBusy);
    connect(vm, &Page3ViewModel::scopeControlsEnabledChanged, this, &Page3::updateScopeControls);

    // Trigger
    connect(this, &Page3::triggerWidgetCreated, vm, &Page3ViewModel::onTriggerWidgetCreated);
    connect(this, &Page3::triggerWidgetDestroyed, vm, &Page3ViewModel::onTriggerWidgetDestroyed);

    // 示波器抓取相關
    connect(this, &Page3::waveformCaptured, vm, &Page3ViewModel::OnWaveformCaptured);
    connect(this, &Page3::csvCaptured, vm, &Page3ViewModel::OnCsvCaptured);
    connect(this, &Page3::allcsvCaptured, vm, &Page3ViewModel::OnAllCsvCaptured);
    connect(this, &Page3::wfmCaptured, vm, &Page3ViewModel::OnWfmCaptured);
    connect(this, &Page3::allwfmCaptured, vm, &Page3ViewModel::OnAllWfmCaptured);

    // Sync dynamic
    if (chkDyload) {
        connect(chkDyload, &QCheckBox::toggled, vm, &Page3ViewModel::onSyncChanged);
    }
}

// 連接輔助函數

void Page3::connectToggleButton(QPushButton* btn, void (Page3::*signal)(bool))
{
    // 更新按鈕文字
    connect(btn, &QPushButton::toggled, this, [btn](bool on) { btn->setText(on ? tr("ON") : tr("OFF")); });

    // 發送信號
    connect(btn, &QPushButton::toggled, this, signal);
}

void Page3::connectComboBox(QComboBox* cmb, TableKind kind)
{
    connect(cmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, cmb, kind](int /*visualIdx*/) {
                int realIdx = cmb->currentData().toInt();
                emit selectedChanged(kind, realIdx, cmb->currentText());
            });
}

void Page3::connectChangeButton(QPushButton* btn, void (Page3::*signal)())
{
    connect(btn, &QPushButton::clicked, this, signal);
}

// UI 輔助

QPushButton* Page3::createPushButton(const QString& text, const QString& objectName) const
{
    auto* btn = new QPushButton(text);
    if (!objectName.isEmpty())
        btn->setObjectName(objectName);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void Page3::applyStyles()
{
    const QString gbQss = QStringLiteral(R"(
        QGroupBox {
            background:#FFF;
            border:1px solid #E0E0E0;
            border-radius:8px;
            margin:0px;
        }
        QGroupBox::title {
            subcontrol-origin:padding;
            left:0px;
            padding:0 3px;
        }
    )");

    const QString btnModern = QStringLiteral(R"(
        QPushButton {
            background: #fff;
            color: #111;
            border: 1.2px solid #bbb;
            border-radius: 4px;
            padding: 2px 0px;
            font-size: 11px;
            min-height: 15px;
            min-width: 32px;
        }
        QPushButton:hover {
            background: #f6f6f6;
            border: 1.5px solid #888;
        }
        QPushButton:pressed {
            background: #86B9EC;
            border: 0px solid #338fec;
        }
        QPushButton:focus {
            outline: none;
            border: 2.2px solid #338fec;
        }
        QPushButton:checked {
            background: #338fec;
            border: 1.5px solid #338fec;
            color: #000;
        }
        QPushButton[outputStatus="unknown"] {
            background: #fff4ce;
            border: 1.5px solid #b7791f;
            color: #704500;
        }
        QPushButton[outputStatus="unknown"]:hover {
            background: #ffe8a3;
        }
        QPushButton[outputStatus="unknown"]:pressed {
            background: #ffdc7a;
        }
        QPushButton[outputStatus="busy"] {
            background: #ededed;
            border: 1.2px solid #bbb;
            color: #555;
        }
    )");

    this->setStyleSheet(gbQss + btnModern);
}

void Page3::loadLock()
{
    // Keep the operation lock active when toggled/forceOff refreshes the interlock.
    const bool loadEnabled = !m_loadOperationBusy && !btnDyloadOn->isChecked();
    const bool dyloadEnabled = !m_loadOperationBusy && !btnLoadOn->isChecked();
    btnLoadOn->setEnabled(loadEnabled);
    btnLoadChg->setEnabled(loadEnabled);
    btnDyloadOn->setEnabled(dyloadEnabled);
    btnDyloadChg->setEnabled(dyloadEnabled);
}

void Page3::setLoadOperationBusy(bool busy)
{
    m_loadOperationBusy = busy;
    loadLock();
}

// Slots

void Page3::onHeadersChanged(const QStringList& hdr)
{
    tblTop->setColumnCount(hdr.size());
    tblTop->setHorizontalHeaderLabels(hdr);

    tblTop->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tblTop->setColumnWidth(0, 80);
    for (int i = 1; i < hdr.size(); ++i)
        tblTop->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
}

void Page3::onRowLabelsChanged(const QStringList& names)
{
    int chCount = tblTop->columnCount() - 1;
    tblTop->setRowCount(3); // Name, Vo, Io

    auto createItem = [](const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled);
        return item;
    };

    // Row 0: Name
    tblTop->setItem(0, 0, createItem("Name"));
    for (int col = 1; col <= chCount; ++col) {
        QString val = (col - 1 < names.size()) ? names[col - 1] : "";
        tblTop->setItem(0, col, createItem(val));
    }

    // Row 1: Vo
    tblTop->setItem(1, 0, createItem("Vo"));
    for (int col = 1; col <= chCount; ++col)
        tblTop->setItem(1, col, createItem(""));

    // Row 2: Io
    tblTop->setItem(2, 0, createItem("Io"));
    for (int col = 1; col <= chCount; ++col)
        tblTop->setItem(2, col, createItem(""));

    tblTop->resizeRowsToContents();
    tblTop->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    tblTop->updateGeometry();
}

void Page3::onTitlesUpdated(TableKind type, const QStringList& titles)
{
    QComboBox* cmb = nullptr;
    switch (type) {
    case TableKind::Input:
        cmb = cmbInput;
        break;
    case TableKind::Load:
        cmb = cmbLoad;
        break;
    case TableKind::DyLoad:
        cmb = cmbDyload;
        break;
    case TableKind::Relay:
        cmb = cmbRelay;
        break;
    default:
        return;
    }
    if (!cmb)
        return;

    int currentIndex = cmb->currentIndex();
    QString currentText = cmb->currentText();

    QSignalBlocker blocker(cmb);
    cmb->clear();
    for (int i = 0; i < titles.size(); ++i) {
        if (!titles[i].trimmed().isEmpty()) {
            cmb->addItem(titles[i], i); // userData = 實際 row index
        }
    }

    // 恢復選擇（優先用文字比對，避免視覺位置偏移）
    if (!currentText.isEmpty()) {
        int foundIndex = cmb->findText(currentText);
        if (foundIndex >= 0) {
            cmb->setCurrentIndex(foundIndex);
        } else if (currentIndex >= 0 && currentIndex < cmb->count()) {
            cmb->setCurrentIndex(currentIndex);
        }
    } else if (currentIndex >= 0 && currentIndex < cmb->count()) {
        cmb->setCurrentIndex(currentIndex);
    }

    if (cmb->currentIndex() >= 0) {
        int realIdx = cmb->currentData().toInt();
        emit selectedChanged(type, realIdx, cmb->currentText());
    }
}

void Page3::forceButtonOff(TableKind type)
{
    setOutputButtonState(type, false);
}

void Page3::setOutputButtonState(TableKind type, bool on)
{
    QPushButton* btn = nullptr;
    switch (type) {
    case TableKind::Input:
        btn = btnInput;
        break;
    case TableKind::Load:
        btn = btnLoadOn;
        break;
    case TableKind::DyLoad:
        btn = btnDyloadOn;
        break;
    case TableKind::Relay:
        btn = btnRelayOn;
        break;
    default:
        return;
    }

    if (btn) {
        QSignalBlocker blocker(btn);
        btn->setChecked(on);
        const bool busy = vm->outputBusy(type);
        const bool unknown = vm->outputState(type) == OutputState::Unknown;
        btn->setText(busy ? tr("Working...") : unknown ? tr("UNKNOWN") : on ? tr("ON") : tr("OFF"));
        btn->setToolTip(unknown && !busy ? tr("Output state is unknown. Click to attempt OFF.") : QString{});
        btn->setEnabled(!busy);
        applyOutputStatusStyle(btn, busy, unknown);
        if (btn == btnLoadOn || btn == btnDyloadOn)
            loadLock();
    }
}

// Trigger 相關

void Page3::setTriggerModel(const QString& modelName)
{
    // qDebug() << "Start Page3::onPage1ConfigChanged setTriggerModel";
    if (m_currentTriggerModel != modelName) {
        m_currentTriggerModel = modelName;
    }
    createTriggerWidget();

    // qDebug() << "End Page3::onPage1ConfigChanged setTriggerModel";
}

void Page3::createTriggerWidget()
{
    // 移除舊的 trigger widget
    if (grpTrigger) {
        if (m_triggerController) {
            m_triggerController->disconnect();
        }

        delete grpTrigger;
        grpTrigger = nullptr;
        m_triggerController = nullptr;

        //通知 ViewModel 控制器已被刪除
        emit triggerWidgetDestroyed();
    }

    // 創建新的 trigger widget
    if (!m_currentTriggerModel.isEmpty()) {
        grpTrigger = TriggerWidgetFactory::createTriggerWidget(
            m_currentTriggerModel, this, &m_triggerController,
            TriggerModelCatalog::channelCount(m_currentTriggerModel));
        if (grpTrigger) {
            m_ctrlLay->addWidget(grpTrigger, 0, Qt::AlignLeft);
            if (m_triggerController) {
                emit triggerWidgetCreated(m_currentTriggerModel, m_triggerController);
            }
        }
    }
}

void Page3::onPage1ConfigChanged(const Page1Config& cfg)
{
    // qDebug() << "Start Page3::onPage1ConfigChanged onPage1ConfigChanged";
    QString triggerModel;
    for (const auto& ic : cfg.instruments) {
        if (ic.type == "Oscilloscope" && ic.enabled) {
            triggerModel = ic.modelName;
            break;
        }
    }
    setTriggerModel(triggerModel);
    const int selectedChannel = cmbCaptureChannel->currentData().toInt();
    const int channelCount = TriggerModelCatalog::isSupported(triggerModel)
                                 ? TriggerModelCatalog::channelCount(triggerModel)
                                 : 0;
    cmbCaptureChannel->clear();
    for (int channel = 1; channel <= channelCount; ++channel)
        cmbCaptureChannel->addItem(tr("CH%1").arg(channel), channel);
    if (channelCount > 0)
        cmbCaptureChannel->setCurrentIndex(selectedChannel >= 1 && selectedChannel <= channelCount
                                               ? selectedChannel - 1
                                               : 0);
    cmbCaptureChannel->setEnabled(channelCount > 0);
    updateScopeControls();
    // qDebug() << "End Page3::onPage1ConfigChanged onPage1ConfigChanged";
}

void Page3::updateScopeControls()
{
    const bool enabled = vm && vm->scopeControlsEnabled();
    grpCap->setEnabled(enabled);
    m_ctrlArea->setEnabled(enabled);
}

// UI 同步

void Page3::syncUIToViewModel()
{
    if (!vm)
        return;

    auto syncCombo = [this](QComboBox* cmb, TableKind kind) {
        if (cmb && cmb->currentIndex() >= 0 && cmb->currentIndex() < cmb->count()) {
            int realIdx = cmb->currentData().toInt();
            emit selectedChanged(kind, realIdx, cmb->currentText());
        }
    };

    syncCombo(cmbInput, TableKind::Input);
    syncCombo(cmbLoad, TableKind::Load);
    syncCombo(cmbDyload, TableKind::DyLoad);
    syncCombo(cmbRelay, TableKind::Relay);
    for (int source = 0; source < 3; ++source)
        vm->onDcSelected(source, cmbDcInput->currentIndex() < 0 ? -1 : cmbDcInput->currentData().toInt(),
                         cmbDcInput->currentText());
}

void Page3::updateDcControls()
{
    const int target = cmbDcTarget->currentData().toInt();
    bool busy = false;
    bool on = false;
    bool unknown = false;
    QStringList states;
    for (int source = 0; source < 3; ++source) {
        busy |= vm->dcBusy(source);
        if (target < 0 || target == source) {
            on |= vm->dcOutputOn(source);
            unknown |= vm->dcOutputState(source) == OutputState::Unknown;
        }
        const auto state = vm->dcOutputState(source);
        const auto text = vm->dcBusy(source) ? tr("Working...") :
                          state == OutputState::Unknown ? tr("UNKNOWN") :
                          state == OutputState::ConfirmedOn ? tr("ON") : tr("OFF");
        states << tr("DC%1: %2").arg(source + 1).arg(text);
    }
    lblDcState->setText(states.join('\n'));
    const QSignalBlocker blocker(btnDcInput);
    // A mixed group offers OFF so an active source can always be turned off.
    btnDcInput->setChecked(on);
    btnDcInput->setText(busy ? tr("Working...") : unknown ? tr("UNKNOWN") : on ? tr("OFF") : tr("ON"));
    btnDcInput->setToolTip(unknown && !busy ? tr("Output state is unknown. Click to attempt OFF.") : QString{});
    btnDcInput->setEnabled(!busy);
    applyOutputStatusStyle(btnDcInput, busy, unknown);
    btnDcChange->setEnabled(!busy);
    cmbDcInput->setEnabled(!busy);
    cmbDcTarget->setEnabled(!busy);
}

void Page3::resetUIFromViewModel()
{
    if (!vm)
        return;

    QTimer::singleShot(50, this, [this]() {
        restoreComboBoxSelection(TableKind::Input, vm->getSelectedInputIndex(), vm->getSelectedInputText());
        restoreComboBoxSelection(TableKind::Load, vm->getSelectedLoadIndex(), vm->getSelectedLoadText());
        restoreComboBoxSelection(TableKind::DyLoad, vm->getSelectedDyLoadIndex(),
                                 vm->getSelectedDyLoadText());
        restoreComboBoxSelection(TableKind::Relay, vm->getSelectedRelayIndex(), vm->getSelectedRelayText());
    });
}

void Page3::restoreComboBoxSelection(TableKind type, int index, const QString& text)
{
    QComboBox* cmb = nullptr;
    switch (type) {
    case TableKind::Input:
        cmb = cmbInput;
        break;
    case TableKind::Load:
        cmb = cmbLoad;
        break;
    case TableKind::DyLoad:
        cmb = cmbDyload;
        break;
    case TableKind::Relay:
        cmb = cmbRelay;
        break;
    default:
        return;
    }

    if (!cmb)
        return;

    QSignalBlocker blocker(cmb);

    // 先按 userData（真實 row index）查找
    for (int i = 0; i < cmb->count(); ++i) {
        if (cmb->itemData(i).toInt() == index) {
            cmb->setCurrentIndex(i);
            return;
        }
    }

    // 退而求其次，按文字查找
    if (!text.isEmpty()) {
        int foundIndex = cmb->findText(text);
        if (foundIndex >= 0) {
            cmb->setCurrentIndex(foundIndex);
        }
    }
}

void Page3::onRestoreSelections(TableKind type, int index, const QString& text)
{
    restoreComboBoxSelection(type, index, text);
}

void Page3::debugCurrentSelections() const
{
    qDebug() << "[Page3] Current UI selections:";
    if (cmbInput) {
        qDebug() << "  Input: index=" << cmbInput->currentIndex() << "text=" << cmbInput->currentText()
                 << "count=" << cmbInput->count();
    }
    if (cmbLoad) {
        qDebug() << "  Load: index=" << cmbLoad->currentIndex() << "text=" << cmbLoad->currentText()
                 << "count=" << cmbLoad->count();
    }
    if (cmbDyload) {
        qDebug() << "  DyLoad: index=" << cmbDyload->currentIndex() << "text=" << cmbDyload->currentText()
                 << "count=" << cmbDyload->count();
    }
}
