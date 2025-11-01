#include "page3.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QHeaderView>
#include <QTimer>
#include "styleutils.h"

// ========== 建構函數 ==========
Page3::Page3(Page3ViewModel* viewModel, QWidget *parent)
    : QWidget(parent), vm(viewModel)
{
    initializeUI();
    buildLayout();
    applyStyles();
    setupConnections();
}

// ========== 初始化 ==========

void Page3::initializeUI()
{
    // Input Group
    btnInput  = createPushButton(tr("ON"));
    btnChange = createPushButton(tr("change"), "btnChange");
    btnInput->setCheckable(true);
    btnInput->setFixedSize(kBtnWidth, kBtnHeight);
    btnChange->setFixedSize(kBtnWidth, kBtnHeight);

    cmbInput = new QComboBox(this);
    cmbInput->setEditable(false);
    cmbInput->setMinimumWidth(kBtnWidth);

    grpInput = new QGroupBox(tr("Input"));
    grpInput->setFont(QFont(font().family(), 9, QFont::Bold));
    grpInput->setFixedWidth(168);

    // Load Group
    btnLoadOn  = createPushButton(tr("ON"));
    btnLoadChg = createPushButton(tr("change"), "btnLoadChg");
    btnLoadOn->setCheckable(true);
    btnLoadOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnLoadChg->setFixedSize(kBtnWidth, kBtnHeight);

    cmbLoad = new QComboBox(this);
    cmbLoad->setEditable(false);
    cmbLoad->setMinimumWidth(kBtnWidth);

    grpLoad = new QGroupBox(tr("Load"));
    grpLoad->setFont(QFont(font().family(), 9, QFont::Bold));
    grpLoad->setFixedWidth(168);

    // Dynamic Load Group
    btnDyloadOn  = createPushButton(tr("ON"));
    btnDyloadChg = createPushButton(tr("change"), "btnLoadChg");
    btnDyloadOn->setCheckable(true);
    btnDyloadOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnDyloadChg->setFixedSize(kBtnWidth, kBtnHeight);

    chkDyload = new QCheckBox(tr("Synchronous"), this);
    chkDyload->setObjectName("chkDyload");

    cmbDyload = new QComboBox(this);
    cmbDyload->setEditable(false);
    cmbDyload->setMinimumWidth(kBtnWidth);

    grpDyload = new QGroupBox(tr("Dynamic Load"));
    grpDyload->setFont(QFont(font().family(), 9, QFont::Bold));
    grpDyload->setFixedWidth(168);

    // Relay Group
    btnRelayOn  = createPushButton(tr("ON"));
    btnRelayChg = createPushButton(tr("change"), "btnRelayChg");
    btnRelayOn->setCheckable(true);
    btnRelayOn->setFixedSize(kBtnWidth, kBtnHeight);
    btnRelayChg->setFixedSize(kBtnWidth, kBtnHeight);

    cmbRelay = new QComboBox(this);
    cmbRelay->setEditable(false);
    cmbRelay->setMinimumWidth(kBtnWidth);

    grpRelay = new QGroupBox(tr("Relay"));
    grpRelay->setFont(QFont(font().family(), 9, QFont::Bold));
    grpRelay->setFixedWidth(168);

    // Capture Group
    btnPic = createPushButton(tr("Waveform"), "btnPic");
    btnCsv = createPushButton(tr("CSV"), "btnCsv");
    btnPic->setFixedSize(kBtnWidth, kBtnHeight);
    btnCsv->setFixedSize(kBtnWidth, kBtnHeight);

    grpCap = new QGroupBox(tr("Capture"));
    grpCap->setFont(QFont(font().family(), 9, QFont::Bold));
    grpCap->setFixedWidth(168);

    // Table
    tblTop = new QTableWidget(this);
    tblTop->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblTop->verticalHeader()->setVisible(false);
    tblTop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 控制區
    m_ctrlArea = new QWidget(this);
    m_ctrlLay  = new QHBoxLayout(m_ctrlArea);
    m_ctrlLay->setContentsMargins(0,0,0,0);
    m_ctrlLay->setSpacing(0);
}

void Page3::buildLayout()
{
    // 創建各組的佈局（使用 lambda 減少重複）
    auto createGroupLayout = [](QGroupBox* grp, QComboBox* cmb, QPushButton* btn1,
                                QPushButton* btn2, QCheckBox* chk = nullptr) {
        auto *lay = new QVBoxLayout(grp);
        lay->setSpacing(6);
        lay->setContentsMargins(4, 16, 4, 8);
        lay->addWidget(cmb);

        if (chk) lay->addWidget(chk);

        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        row->addWidget(btn1);
        row->addWidget(btn2);
        lay->addLayout(row);
    };

    createGroupLayout(grpInput, cmbInput, btnInput, btnChange);
    createGroupLayout(grpLoad, cmbLoad, btnLoadOn, btnLoadChg);
    createGroupLayout(grpDyload, cmbDyload, btnDyloadOn, btnDyloadChg, chkDyload);
    createGroupLayout(grpRelay, cmbRelay, btnRelayOn, btnRelayChg);

    // Capture Group
    auto *layCap = new QVBoxLayout(grpCap);
    layCap->setSpacing(6);
    layCap->setContentsMargins(4, 18, 4, 8);
    auto *rowCap = new QHBoxLayout;
    rowCap->setSpacing(6);
    rowCap->addWidget(btnPic);
    rowCap->addWidget(btnCsv);
    layCap->addLayout(rowCap);

    // 左欄
    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(10);
    leftCol->addWidget(grpInput);
    leftCol->addWidget(grpLoad);
    leftCol->addWidget(grpDyload);
    leftCol->addWidget(grpRelay);
    leftCol->addStretch();
    leftCol->addWidget(grpCap);

    auto *leftFrame = new QFrame;
    leftFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
    leftFrame->setLineWidth(1);
    leftFrame->setLayout(leftCol);

    // 右欄
    auto *rightFrame = new QFrame;
    rightFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
    rightFrame->setLineWidth(1);

    auto *rightLay = new QVBoxLayout(rightFrame);
    rightLay->setContentsMargins(0,0,0,0);
    rightLay->setSpacing(4);
    rightLay->addWidget(m_ctrlArea);

    // 主佈局
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(4);
    root->addWidget(leftFrame);
    root->addWidget(rightFrame, 1);
}

void Page3::setupConnections()
{
    // Toggle 按鈕連接（使用輔助函數減少重複）
    connectToggleButton(btnInput, &Page3::inputToggled);
    connectToggleButton(btnLoadOn, &Page3::loadToggled);
    connectToggleButton(btnDyloadOn, &Page3::dyloadToggled);

    // ComboBox 選擇變更（使用輔助函數）
    connectComboBox(cmbInput, LoadKind::Input);
    connectComboBox(cmbLoad, LoadKind::Load);
    connectComboBox(cmbDyload, LoadKind::DyLoad);
    connectComboBox(cmbRelay, LoadKind::Relay);

    // Change 按鈕連接（使用輔助函數）
    connectChangeButton(btnChange, &Page3::inputChanged);
    connectChangeButton(btnLoadChg, &Page3::loadChanged);
    connectChangeButton(btnDyloadChg, &Page3::dyloadChanged);

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

    connect(vm, &Page3ViewModel::forceOff, this, &Page3::forceButtonOff);
    connect(vm, &Page3ViewModel::page1ConfigChanged, this, &Page3::onPage1ConfigChanged);
    connect(vm, &Page3ViewModel::headersChanged, this, &Page3::onHeadersChanged);
    connect(vm, &Page3ViewModel::rowLabelsChanged, this, &Page3::onRowLabelsChanged);
    connect(vm, &Page3ViewModel::TitlesUpdated, this, &Page3::onTitlesUpdated);
    connect(vm, &Page3ViewModel::restoreSelections, this, &Page3::onRestoreSelections);

    // Trigger
    connect(this, &Page3::triggerWidgetCreated, vm, &Page3ViewModel::onTriggerWidgetCreated);
    connect(this, &Page3::triggerWidgetDestroyed, vm, &Page3ViewModel::onTriggerWidgetDestroyed);
}

// ========== 連接輔助函數 ==========

void Page3::connectToggleButton(QPushButton* btn, void (Page3::*signal)(bool))
{
    // 更新按鈕文字
    connect(btn, &QPushButton::toggled, this, [btn](bool on) {
        btn->setText(on ? tr("ON") : tr("OFF"));
    });

    // 發送信號
    connect(btn, &QPushButton::toggled, this, signal);
}

void Page3::connectComboBox(QComboBox* cmb, LoadKind kind)
{
    connect(cmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, cmb, kind](int idx) {
                emit selectedChanged(kind, idx, cmb->currentText());
            });
}

void Page3::connectChangeButton(QPushButton* btn, void (Page3::*signal)())
{
    connect(btn, &QPushButton::clicked, this, signal);
}

// ========== UI 輔助 ==========

QPushButton* Page3::createPushButton(const QString &text, const QString &objectName) const
{
    auto *btn = new QPushButton(text);
    if (!objectName.isEmpty()) btn->setObjectName(objectName);
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
    )");

    this->setStyleSheet(gbQss + btnModern);
}

void Page3::loadLock()
{
    // Load 和 DyLoad 互鎖
    btnDyloadOn->setEnabled(!btnLoadOn->isChecked());
    btnDyloadChg->setEnabled(!btnLoadOn->isChecked());
    btnLoadOn->setEnabled(!btnDyloadOn->isChecked());
    btnLoadChg->setEnabled(!btnDyloadOn->isChecked());
}

// ========== Slots ==========

void Page3::onHeadersChanged(const QStringList &hdr)
{
    tblTop->setColumnCount(hdr.size());
    tblTop->setHorizontalHeaderLabels(hdr);

    tblTop->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tblTop->setColumnWidth(0, 80);
    for (int i = 1; i < hdr.size(); ++i)
        tblTop->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
}

void Page3::onRowLabelsChanged(const QStringList &names)
{
    int chCount = tblTop->columnCount() - 1;
    tblTop->setRowCount(3); // Name, Vo, Io

    auto createItem = [](const QString& text) {
        auto *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled);
        return item;
    };

    // Row 0: Name
    tblTop->setItem(0, 0, createItem("Name"));
    for (int col = 1; col <= chCount; ++col) {
        QString val = (col-1 < names.size()) ? names[col-1] : "";
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

void Page3::onTitlesUpdated(LoadKind type, const QStringList& titles)
{
    QComboBox* cmb = nullptr;
    switch (type) {
    case LoadKind::Input:   cmb = cmbInput;   break;
    case LoadKind::Load:    cmb = cmbLoad;    break;
    case LoadKind::DyLoad:  cmb = cmbDyload;  break;
    case LoadKind::Relay:   cmb = cmbRelay;   break;
    default: return;
    }
    if (!cmb) return;

    int currentIndex = cmb->currentIndex();
    QString currentText = cmb->currentText();

    QSignalBlocker blocker(cmb);
    cmb->clear();
    for (const QString& t : titles) {
        if (!t.trimmed().isEmpty())
            cmb->addItem(t);
    }

    // 恢復選擇
    if (currentIndex >= 0 && currentIndex < cmb->count()) {
        cmb->setCurrentIndex(currentIndex);
    } else if (!currentText.isEmpty()) {
        int foundIndex = cmb->findText(currentText);
        if (foundIndex >= 0) {
            cmb->setCurrentIndex(foundIndex);
        }
    }

    if (cmb->currentIndex() >= 0) {
        emit selectedChanged(type, cmb->currentIndex(), cmb->currentText());
    }
}

void Page3::forceButtonOff(LoadKind type)
{
    QPushButton* btn = nullptr;
    switch (type) {
    case LoadKind::Input:   btn = btnInput;     break;
    case LoadKind::Load:    btn = btnLoadOn;    break;
    case LoadKind::DyLoad:  btn = btnDyloadOn;  break;
    default: return;
    }

    if (btn) {
        QSignalBlocker blocker(btn);
        btn->setChecked(false);
        qDebug() << "forceButtonOff";
        if (btn == btnLoadOn || btn == btnDyloadOn)
            loadLock();
    }
}

// ========== Trigger 相關 ==========

void Page3::setTriggerModel(const QString& modelName)
{
    qDebug() << "Start Page3::onPage1ConfigChanged setTriggerModel";
    if (m_currentTriggerModel != modelName) {
        m_currentTriggerModel = modelName;
    }
    createTriggerWidget();

    qDebug() << "End Page3::onPage1ConfigChanged setTriggerModel";
}

void Page3::createTriggerWidget()
{
    qDebug() << "Start Page3::onPage1ConfigChanged createTriggerWidget";
    // 移除舊的 trigger widget
    if (grpTrigger) {
        // 先斷開信號
        if (m_triggerController) {
            m_triggerController->disconnect();
        }

        // 刪除 widget（會自動刪除子控制器）
        delete grpTrigger;
        grpTrigger = nullptr;

        // 清空指標（不要再次刪除）
        m_triggerController = nullptr;
    }

    // 創建新的 trigger widget
    if (!m_currentTriggerModel.isEmpty()) {
        grpTrigger = TriggerWidgetFactory::createTriggerWidget(
            m_currentTriggerModel, this, &m_triggerController);
        if (grpTrigger) {
            m_ctrlLay->addWidget(grpTrigger, 0, Qt::AlignLeft);
            if (m_triggerController) {
                emit triggerWidgetCreated(m_currentTriggerModel, m_triggerController);
            }
        }
    }
    qDebug() << "End Page3::onPage1ConfigChanged createTriggerWidget";
}

void Page3::onPage1ConfigChanged(const Page1Config &cfg)
{
    qDebug() << "Start Page3::onPage1ConfigChanged onPage1ConfigChanged";
    QString triggerModel;
    for (const auto& ic : cfg.instruments) {
        if (ic.type == "Oscilloscope" && ic.enabled) {
            triggerModel = ic.modelName;
            break;
        }
    }
        setTriggerModel(triggerModel);
    qDebug() << "End Page3::onPage1ConfigChanged onPage1ConfigChanged";
}

// ========== UI 同步 ==========

void Page3::syncUIToViewModel()
{
    if (!vm) return;

    auto syncCombo = [this](QComboBox* cmb, LoadKind kind) {
        if (cmb && cmb->currentIndex() >= 0 && cmb->currentIndex() < cmb->count()) {
            emit selectedChanged(kind, cmb->currentIndex(), cmb->currentText());
        }
    };

    syncCombo(cmbInput, LoadKind::Input);
    syncCombo(cmbLoad, LoadKind::Load);
    syncCombo(cmbDyload, LoadKind::DyLoad);
    syncCombo(cmbRelay, LoadKind::Relay);
}

void Page3::resetUIFromViewModel()
{
    if (!vm) return;

    QTimer::singleShot(50, this, [this]() {
        restoreComboBoxSelection(LoadKind::Input, vm->getSelectedInputIndex(), vm->getSelectedInputText());
        restoreComboBoxSelection(LoadKind::Load, vm->getSelectedLoadIndex(), vm->getSelectedLoadText());
        restoreComboBoxSelection(LoadKind::DyLoad, vm->getSelectedDyLoadIndex(), vm->getSelectedDyLoadText());
        restoreComboBoxSelection(LoadKind::Relay, vm->getSelectedRelayIndex(), vm->getSelectedRelayText());
    });
}

void Page3::restoreComboBoxSelection(LoadKind type, int index, const QString& text)
{
    QComboBox* cmb = nullptr;
    switch (type) {
    case LoadKind::Input:   cmb = cmbInput;   break;
    case LoadKind::Load:    cmb = cmbLoad;    break;
    case LoadKind::DyLoad:  cmb = cmbDyload;  break;
    case LoadKind::Relay:   cmb = cmbRelay;   break;
    default: return;
    }

    if (!cmb) return;

    QSignalBlocker blocker(cmb);

    // 先按索引恢復
    if (index >= 0 && index < cmb->count()) {
        cmb->setCurrentIndex(index);
        return;
    }

    // 按文字查找
    if (!text.isEmpty()) {
        int foundIndex = cmb->findText(text);
        if (foundIndex >= 0) {
            cmb->setCurrentIndex(foundIndex);
        }
    }
}

void Page3::onRestoreSelections(LoadKind type, int index, const QString& text)
{
    restoreComboBoxSelection(type, index, text);
}

void Page3::debugCurrentSelections() const
{
    qDebug() << "[Page3] Current UI selections:";
    if (cmbInput) {
        qDebug() << "  Input: index=" << cmbInput->currentIndex()
        << "text=" << cmbInput->currentText()
        << "count=" << cmbInput->count();
    }
    if (cmbLoad) {
        qDebug() << "  Load: index=" << cmbLoad->currentIndex()
        << "text=" << cmbLoad->currentText()
        << "count=" << cmbLoad->count();
    }
    if (cmbDyload) {
        qDebug() << "  DyLoad: index=" << cmbDyload->currentIndex()
        << "text=" << cmbDyload->currentText()
        << "count=" << cmbDyload->count();
    }
}
