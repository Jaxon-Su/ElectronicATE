#include "page2.h"
#include "page2viewmodel.h"
#include "styleutils.h"
#include "navdelegate.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QLabel>
#include <QShortcut>
#include <QMenu>
#include <algorithm>
#include <cmath>

// ========== 構造函數 ==========

Page2::Page2(Page2ViewModel* viewModel, QWidget *parent)
    : QWidget(parent), vm(viewModel)
{
    initializeUi();
    setupLayouts();
    setupConnections();
    setupDelegates();
    setupInitialTableState();
}

// ========== 公共方法 ==========

void Page2::syncUIToViewModel()
{
    syncInputTable();
    syncDcTable();
    syncRelayTable();
    syncLoadTable();
    syncDynamicTable();
}

// ========== 表格工具函數 ==========

QTableWidget* Page2::tableByKind(TableKind k) const
{
    switch(k) {
    case TableKind::Input:   return tblInput;
    case TableKind::Dc:      return tblDc;
    case TableKind::Relay:   return tblRelay;
    case TableKind::Load:    return tblLoad;
    case TableKind::DyLoad:  return tblDynamic;
    }
    return nullptr;
}

TableKind Page2::kindOf(const QTableWidget* tbl) const
{
    if (tbl == tblInput)    return TableKind::Input;
    if (tbl == tblDc)       return TableKind::Dc;
    if (tbl == tblRelay)    return TableKind::Relay;
    if (tbl == tblLoad)     return TableKind::Load;
    return TableKind::DyLoad;
}

// ========== Meta 行建立 ==========

void Page2::ensureRelayMetaRows(int maxOutput)
{
    // Relay 表格不需要 Meta 行
}

void Page2::createMetaHeaderLabel(QTableWidget* tbl, int row, const QString& text)
{
    if (!tbl->item(row, 1)) {
        auto *item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        tbl->setItem(row, 1, item);
    }
    tbl->item(row, 1)->setText(text);
}

void Page2::ensureLoadMetaRows(int maxOutput)
{
    while (tblLoad->rowCount() < kMetaRowsLoad)
        tblLoad->insertRow(tblLoad->rowCount());

    createMetaHeaderLabel(tblLoad, 0, "Mode");
    createMetaHeaderLabel(tblLoad, 1, "Range");
    createMetaHeaderLabel(tblLoad, 2, "Name");
    createMetaHeaderLabel(tblLoad, 3, "Vo");
    createMetaHeaderLabel(tblLoad, 4, "Von");

    const QStringList modes{"CC", "CV"};

    for (int col = 2; col <= maxOutput + 1; ++col) {
        QComboBox *cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col));
        if (!cb) {
            cb = new QComboBox(tblLoad);
            cb->addItems(modes);
            tblLoad->setCellWidget(0, col, cb);
            connect(cb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                    this, [=] {
                        this->syncUIToViewModel();
                        vm->cellValueChanged(TableKind::Load, 0, col - 1, cb->currentText());
                        updateLoadRangeCellOptions(col - 1);
                        this->syncUIToViewModel();
                        vm->broadcastAllPowers();
                    });
        } else {
            for (const auto& mode : modes) {
                if (cb->findText(mode) < 0)
                    cb->addItem(mode);
            }
        }
        cb->installEventFilter(this);
        StyleUtils::applyComboBoxStyle(cb, true);

        for (int row = 1; row < kMetaRowsLoad; ++row) {
            if (tblLoad->cellWidget(row, col))
                continue;
            if (row == 1) {
                QComboBox *rangeCb = new QComboBox(tblLoad);
                rangeCb->addItems(vm->loadRangeOptions(col - 1, cb->currentText()));
                tblLoad->setCellWidget(row, col, rangeCb);
                connect(rangeCb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                        this, [=] {
                            this->syncUIToViewModel();
                            vm->cellValueChanged(TableKind::Load, row, col - 1, rangeCb->currentText());
                        });
                rangeCb->installEventFilter(this);
                StyleUtils::applyComboBoxStyle(rangeCb, true);
                continue;
            }
            QChar tag;
            if (row == 2) {
                tag = QChar();
            } else {
                tag = 'd';
            }
            makeLineEdit(tblLoad, row, col, tag, this, vm, TableKind::Load);
        }
    }
}

void Page2::ensureDynamicMetaRows(int maxOutput)
{
    while (tblDynamic->rowCount() < kMetaRowsDynamic)
        tblDynamic->insertRow(tblDynamic->rowCount());

    createMetaHeaderLabel(tblDynamic, 0, "Range");
    createMetaHeaderLabel(tblDynamic, 1, "Vo");
    createMetaHeaderLabel(tblDynamic, 2, "Von");

    for (int col = 2; col <= maxOutput + 1; ++col) {
        for (int row = 0; row < kMetaRowsDynamic; ++row) {
            if (!tblDynamic->cellWidget(row, col)) {
                if (row == 0) {
                    QComboBox *rangeCb = new QComboBox(tblDynamic);
                    rangeCb->addItems(vm->dynamicRangeOptions(col - 1));
                    tblDynamic->setCellWidget(row, col, rangeCb);
                    connect(rangeCb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                            this, [=] {
                                this->syncUIToViewModel();
                                vm->cellValueChanged(TableKind::DyLoad, row, col - 1, rangeCb->currentText());
                            });
                    rangeCb->installEventFilter(this);
                    StyleUtils::applyComboBoxStyle(rangeCb, true);
                } else {
                    makeLineEdit(tblDynamic, row, col, 'd', this, vm, TableKind::DyLoad);
                }
            }
        }
    }
}

void Page2::ensurePowerColumn(int maxOutput)
{
    const int powerCol = maxOutput + 2;
    const int expectedCols = powerCol + 1;

    if (tblLoad->columnCount() != expectedCols)
        tblLoad->setColumnCount(expectedCols);

    tblLoad->setHorizontalHeaderItem(powerCol, new QTableWidgetItem("Power"));

    for (int row = 0; row < tblLoad->rowCount(); ++row) {
        if (QWidget *widget = tblLoad->cellWidget(row, powerCol)) {
            tblLoad->removeCellWidget(row, powerCol);
            widget->deleteLater();
        }

        QTableWidgetItem *item = tblLoad->item(row, powerCol);
        if (!item) {
            item = new QTableWidgetItem;
            tblLoad->setItem(row, powerCol, item);
        }
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        item->setText("");
    }
}

// ========== 行擴展與創建 ==========

void Page2::createRowWidgets(QTableWidget *tbl, const QStringList &tags)
{
    int row = tbl->rowCount();
    tbl->insertRow(row);
    createRowWidgetsAt(tbl, row, tags);
}

void Page2::createRowWidgetsAt(QTableWidget *tbl, int row, const QStringList &tags)
{
    // col 0 = Seq
    if (!tbl->item(row, 0)) {
        auto *seqItem = new QTableWidgetItem;
        seqItem->setTextAlignment(Qt::AlignCenter);
        seqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tbl->setItem(row, 0, seqItem);
    }

    TableKind kind = kindOf(tbl);

    // DC Table: col 1 = DC Input label (non-editable item), col 2 = Vin (QLineEdit)
    if (tbl == tblDc) {
        if (!tbl->item(row, 1)) {
            auto *inputItem = new QTableWidgetItem;
            inputItem->setTextAlignment(Qt::AlignCenter);
            inputItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            tbl->setItem(row, 1, inputItem);
        }
        if (!tbl->cellWidget(row, 2)) {
            QLineEdit *leVin = makeLineEdit(tbl, row, 2, 'd', this, vm, TableKind::Dc);
            connect(leVin, &QLineEdit::textChanged, this, [=](const QString& text) {
                if (auto *item = tblDc->item(row, 1))
                    item->setText(text);
            });
        }
        refreshSeqCol(tbl);
        return;
    }

    // col 1 = Label
    if (tbl == tblInput) {
        auto *titleItem = new QTableWidgetItem;
        titleItem->setTextAlignment(Qt::AlignCenter);
        titleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tbl->setItem(row, 1, titleItem);
    } else {
        if (!tbl->cellWidget(row, 1)) {
            QLineEdit *leLabel = makeLineEdit(tbl, row, 1, QChar(), this, vm, kind);
            connect(leLabel, &QLineEdit::textChanged, this, [=] {
                this->syncUIToViewModel();
                vm->cellValueChanged(kind, row, 0, leLabel->text());
            });
        }
    }

    // col 2..tags.size()+1 = data widgets
    for (int col = 2; col <= tags.size() + 1; ++col) {
        if (tbl->cellWidget(row, col)) continue;
        if (kind == TableKind::Input && col == 2) {
            makeInputPhaseComboBox(tbl, row, col, this, vm);
        } else if (kind == TableKind::Relay) {
            makeComboBox(tbl, row, col, this, vm, kind);
        } else {
            QChar tag = determineValidatorTag(tbl, row, col, tags);
            makeLineEdit(tbl, row, col, tag, this, vm, kind);
        }
    }

    // Dynamic: T1~T2 at last col
    if (tbl == tblDynamic && row >= kMetaRowsDynamic) {
        int t1t2Col = tags.size() + 2;
        if (!tbl->cellWidget(row, t1t2Col))
            makeLineEdit(tbl, row, t1t2Col, 'r', this, vm, TableKind::DyLoad);

        int powerCol = tags.size() + 3;
        if (!tbl->item(row, powerCol)) {
            auto *powerItem = new QTableWidgetItem;
            powerItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            powerItem->setTextAlignment(Qt::AlignCenter);
            tbl->setItem(row, powerCol, powerItem);
        }
    }

    // Load: Power item at last col
    if (tbl == tblLoad) {
        int powerCol = tags.size() + 2;
        if (!tbl->item(row, powerCol)) {
            auto *powerItem = new QTableWidgetItem;
            powerItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            powerItem->setTextAlignment(Qt::AlignCenter);
            tbl->setItem(row, powerCol, powerItem);
        }
    }

    refreshSeqCol(tbl);
}

QChar Page2::determineValidatorTag(QTableWidget* tbl, int row, int col, const QStringList& tags) const
{
    if (tbl == tblLoad && row == 2)
        return QChar();

    if (tbl == tblDynamic) {
        if (row < kMetaRowsDynamic)
            return 'd';
        int maxOutput = tags.size();
        if (col == maxOutput + 2)
            return 'r';
        else if (col == 1)
            return QChar();
        else
            return 'r';
    }

    int logicalIdx = col - 2;
    if (logicalIdx < 0 || logicalIdx >= tags.size()) return QChar();
    return tags[logicalIdx].isEmpty() ? QChar() : tags[logicalIdx][0];
}

void Page2::extendRows(TableKind kind, const QStringList &tags)
{
    QTableWidget* tbl = tableByKind(kind);
    int rows = tbl->rowCount();

    for (int row = 0; row < rows; ++row) {
        if (shouldSkipMetaRow(tbl, row))
            continue;

        for (int col = 2; col <= tags.size() + 1; ++col) {
            if (tbl->cellWidget(row, col))
                continue;
            int logIdx = col - 2;
            if (kind == TableKind::Input && col == 2) {
                makeInputPhaseComboBox(tbl, row, col, this, vm);
            } else if (kind == TableKind::Relay) {
                makeComboBox(tbl, row, col, this, vm, kind);
            } else {
                QChar tag = logIdx < tags.size() && !tags[logIdx].isEmpty() ? tags[logIdx][0] : QChar();
                makeLineEdit(tbl, row, col, tag, this, vm, kind);
            }
        }
    }
}

bool Page2::shouldSkipMetaRow(QTableWidget* tbl, int row) const
{
    if (tbl == tblLoad && row < kMetaRowsLoad)
        return true;
    if (tbl == tblDynamic && row < kMetaRowsDynamic)
        return true;
    // Input / Dc / Relay: no meta rows
    return false;
}

// ========== Widget 工廠方法 ==========

QValidator* Page2::makeDoubleVal(QObject* parent, int decimals)
{
    auto* validator = new QDoubleValidator(parent);
    validator->setDecimals(decimals);
    validator->setNotation(QDoubleValidator::StandardNotation);
    return validator;
}

QValidator* Page2::makeRangeVal(QObject* parent)
{
    static QRegularExpression regex(R"(^\d+(\.\d{0,10})?~\d+(\.\d{0,10})?$)");
    return new QRegularExpressionValidator(regex, parent);
}

QValidator* Page2::makeLoadValueVal(QObject* parent)
{
    static QRegularExpression regex(
        R"(^\s*\d+(\.\d{0,10})?\s*([aA])?\s*(/\s*\d+(\.\d{0,10})?\s*([aA])?|[vV]\s*/\s*\d+(\.\d{0,10})?\s*([aA])?)?\s*$)");
    return new QRegularExpressionValidator(regex, parent);
}

QLineEdit* Page2::makeLineEdit(QTableWidget* tbl, int r, int c, QChar tag,
                               Page2* self, Page2ViewModel* vm, TableKind kind)
{
    auto* lineEdit = new QLineEdit(tbl);
    lineEdit->setAlignment(Qt::AlignCenter);
    lineEdit->installEventFilter(self);

    if (tag == 'd')
        lineEdit->setValidator(makeDoubleVal(lineEdit, 3));
    else if (tag == 'r')
        lineEdit->setValidator(makeRangeVal(lineEdit));
    else if (tag == 'l')
        lineEdit->setValidator(makeLoadValueVal(lineEdit));

    StyleUtils::applyLineEditStyle(lineEdit);
    tbl->setCellWidget(r, c, lineEdit);

    QObject::connect(lineEdit, &QLineEdit::textChanged, self, [=] {
        self->syncUIToViewModel();
        vm->cellValueChanged(kind, r, c - 1, lineEdit->text());
        if (kind == TableKind::Load)
            vm->broadcastAllPowers();
        else if (kind == TableKind::DyLoad)
            vm->broadcastAllDynamicPowers();
    });

    return lineEdit;
}

QComboBox* Page2::makeComboBox(QTableWidget* tbl, int r, int c,
                               Page2* self, Page2ViewModel* vm, TableKind kind)
{
    auto* comboBox = new QComboBox(tbl);
    comboBox->addItems({"off", "on"});
    comboBox->setCurrentText("off");
    comboBox->installEventFilter(self);
    StyleUtils::applyComboBoxStyle(comboBox, true);
    tbl->setCellWidget(r, c, comboBox);

    QObject::connect(comboBox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                     self, [=] {
                         self->syncUIToViewModel();
                         vm->cellValueChanged(kind, r, c - 1, comboBox->currentText());
                     });

    return comboBox;
}

QComboBox* Page2::makeInputPhaseComboBox(QTableWidget* tbl, int r, int c,
                                         Page2* self, Page2ViewModel* vm)
{
    auto* comboBox = new QComboBox(tbl);
    comboBox->addItems({"1phase", "3phase"});
    comboBox->setCurrentText("1phase");
    comboBox->installEventFilter(self);
    StyleUtils::applyComboBoxStyle(comboBox, true);
    tbl->setCellWidget(r, c, comboBox);

    QObject::connect(comboBox, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                     self, [=] {
                         self->syncUIToViewModel();
                         vm->cellValueChanged(TableKind::Input, r, c - 1, comboBox->currentText());
                     });

    return comboBox;
}

// ========== 同步輔助函數 ==========

void Page2::syncInputTable()
{
    QVector<InputRow> inputs;
    for (int row = 0; row < tblInput->rowCount(); ++row) {
        InputRow inputRow;
        if (auto *cb = qobject_cast<QComboBox*>(tblInput->cellWidget(row, 2)))
            inputRow.phaseMode = cb->currentText();
        if (inputRow.phaseMode.isEmpty())
            inputRow.phaseMode = "1phase";
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 3)))
            inputRow.vin = le->text();
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 4)))
            inputRow.frequency = le->text();
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 5)))
            inputRow.phase = le->text();
        inputs.append(inputRow);
    }
    emit inputRowsChanged(inputs);
}

// DC Table sync
// 結構：col 0 = Seq, col 1 = DC Input (label item), col 2 = Vin (QLineEdit)
void Page2::syncDcTable()
{
    QVector<DcRow> dcRows;
    for (int row = 0; row < tblDc->rowCount(); ++row) {
        DcRow dcRow;
        if (auto *le = qobject_cast<QLineEdit*>(tblDc->cellWidget(row, 2)))
            dcRow.vin = le->text();
        dcRows.append(dcRow);
    }
    emit dcRowsChanged(dcRows);
}

void Page2::syncRelayTable()
{
    QVector<RelayDataRow> relayRows;
    int maxRelayOutput = tblRelay->columnCount() - 2;

    for (int row = kMetaRowsRelay; row < tblRelay->rowCount(); ++row) {
        RelayDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblRelay->cellWidget(row, 1)))
            dataRow.label = le->text();
        else if (auto *item = tblRelay->item(row, 1))
            dataRow.label = item->text();

        for (int col = 2; col <= maxRelayOutput + 1; ++col) {
            if (auto *cb = qobject_cast<QComboBox*>(tblRelay->cellWidget(row, col)))
                dataRow.values << cb->currentText();
            else
                dataRow.values << QString("off");
        }

        while (dataRow.values.size() < maxRelayOutput)
            dataRow.values << QString("off");
        if (dataRow.values.size() > maxRelayOutput)
            dataRow.values.resize(maxRelayOutput);

        relayRows.append(dataRow);
    }
    emit relayRowsChanged(relayRows);
}

void Page2::syncLoadTable()
{
    int maxOutput = tblLoad->columnCount() - 3;

    LoadMetaRow meta;
    for (int col = 2; col <= maxOutput + 1; ++col) {
        if (auto *cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col)))
            meta.modes << cb->currentText();
        if (auto *cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(1, col)))
            meta.ranges << cb->currentText();
    }

    meta.names = extractMetaRowValues(tblLoad, 2, maxOutput);
    meta.vo    = extractMetaRowValues(tblLoad, 3, maxOutput);
    meta.von   = extractMetaRowValues(tblLoad, 4, maxOutput);

    QVector<LoadDataRow> loadRows;
    for (int row = kMetaRowsLoad; row < tblLoad->rowCount(); ++row) {
        LoadDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, 1)))
            dataRow.label = le->text();
        else if (auto *item = tblLoad->item(row, 1))
            dataRow.label = item->text();

        for (int col = 2; col <= maxOutput + 1; ++col) {
            if (auto *le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, col)))
                dataRow.values << le->text();
            else
                dataRow.values << QString();
        }

        while (dataRow.values.size() < maxOutput)
            dataRow.values << QString();
        if (dataRow.values.size() > maxOutput)
            dataRow.values.resize(maxOutput);

        loadRows.append(dataRow);
    }
    emit loadRowsChanged(loadRows);
    emit loadMetaChanged(meta);
}

void Page2::syncDynamicTable()
{
    int dMaxOutput = tblDynamic->columnCount() - 4;

    DynamicMetaRow dmeta;
    for (int col = 2; col <= dMaxOutput + 1; ++col) {
        if (auto *cb = qobject_cast<QComboBox*>(tblDynamic->cellWidget(0, col)))
            dmeta.ranges << cb->currentText();
        else
            dmeta.ranges << QString();
    }
    dmeta.vo  = extractMetaRowValues(tblDynamic, 1, dMaxOutput);
    dmeta.von = extractMetaRowValues(tblDynamic, 2, dMaxOutput);

    int t1t2Col = dMaxOutput + 2;
    dmeta.t1t2.clear();
    for (int row = kMetaRowsDynamic; row < tblDynamic->rowCount(); ++row) {
        if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, t1t2Col)))
            dmeta.t1t2.append(le->text());
        else
            dmeta.t1t2.append(QString());
    }

    QVector<DynamicDataRow> dynamicRows;
    for (int row = kMetaRowsDynamic; row < tblDynamic->rowCount(); ++row) {
        DynamicDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, 1)))
            dataRow.label = le->text();
        else if (auto *item = tblDynamic->item(row, 1))
            dataRow.label = item->text();

        for (int col = 2; col <= dMaxOutput + 1; ++col) {
            if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col)))
                dataRow.values << le->text();
            else
                dataRow.values << QString();
        }

        dynamicRows.append(dataRow);
    }
    emit dynamicRowsChanged(dynamicRows);
    emit dynamicMetaChanged(dmeta);
}

QVector<QString> Page2::extractMetaRowValues(QTableWidget* tbl, int row, int maxOutput)
{
    QVector<QString> result;
    for (int col = 2; col <= maxOutput + 1; ++col) {
        if (auto *le = qobject_cast<QLineEdit*>(tbl->cellWidget(row, col)))
            result.append(le->text());
        else
            result.append(QString());
    }
    return result;
}

// ========== 事件處理 ==========

bool Page2::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, ev);

    auto *keyEvent = static_cast<QKeyEvent*>(ev);

    // Delete/Backspace on table → delete selected data rows
    for (auto *tbl : {tblInput, tblDc, tblRelay, tblLoad, tblDynamic}) {
        if (obj == tbl) {
            if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
                deleteSelectedDataRows(tbl);
                return true;
            }
        }
    }

    // Navigation keys inside QLineEdit children
    auto *lineEdit = qobject_cast<QLineEdit*>(obj);
    if (!lineEdit)
        return QWidget::eventFilter(obj, ev);

    QTableWidget *tbl = findTableForWidget(lineEdit);
    if (!tbl)
        return QWidget::eventFilter(obj, ev);

    QPoint position = lineEdit->mapTo(tbl->viewport(), QPoint(0, 0));
    QModelIndex index = tbl->indexAt(position);
    if (!index.isValid())
        return QWidget::eventFilter(obj, ev);

    return handleNavigationKey(keyEvent, tbl, index.row(), index.column());
}

QTableWidget* Page2::findTableForWidget(QWidget* widget) const
{
    if (tblInput->isAncestorOf(widget))    return tblInput;
    if (tblDc->isAncestorOf(widget))       return tblDc;
    if (tblRelay->isAncestorOf(widget))    return tblRelay;
    if (tblLoad->isAncestorOf(widget))     return tblLoad;
    if (tblDynamic->isAncestorOf(widget))  return tblDynamic;
    return nullptr;
}

bool Page2::handleNavigationKey(QKeyEvent* keyEvent, QTableWidget* tbl, int row, int col)
{
    int maxRow = tbl->rowCount() - 1;
    int maxCol = tbl->columnCount() - 1;

    auto focusCell = [tbl](int r, int c) {
        if (QWidget *widget = tbl->cellWidget(r, c))
            widget->setFocus();
        else {
            tbl->setCurrentCell(r, c);
            tbl->editItem(tbl->item(r, c));
        }
    };

    switch (keyEvent->key()) {
    case Qt::Key_Right:
        if (col < maxCol) { focusCell(row, col + 1); return true; }
        break;
    case Qt::Key_Left:
        if (col > 1) { focusCell(row, col - 1); return true; }
        break;
    case Qt::Key_Down:
        if (row < maxRow) { focusCell(row + 1, col); return true; }
        break;
    case Qt::Key_Up:
        if (row > 0) { focusCell(row - 1, col); return true; }
        break;
    default:
        break;
    }

    return false;
}

// ========== Slots ==========

void Page2::onHeadersChanged(TableKind kind, const QStringList &headers)
{
    if (kind == TableKind::DyLoad) {
        handleDynamicHeadersTime(headers);
        return;
    }

    setupTableHeaders(kind, headers);

    if (kind == TableKind::Relay) {
        handleRelayHeaders(headers.size() - 1);
    } else if (kind == TableKind::Load) {
        handleLoadHeaders();
    } else if (kind != TableKind::Input && kind != TableKind::Dc) {
        // Dc 不在此處理（固定 2 欄，不需要 extendRows）
        QStringList tags(tableByKind(kind)->columnCount() - 1,
                         kind == TableKind::Load ? "d" : "r");
        extendRows(kind, tags);
    }
}

void Page2::handleDynamicHeadersTime(const QStringList &headers)
{
    int newMaxOutput = headers.size() - 3;
    if (newMaxOutput < 0) return;

    const QSignalBlocker blocker(tblDynamic);

    setupTableHeaders(TableKind::DyLoad, headers);
    ensureDynamicMetaRows(newMaxOutput);
    ensureT1T2ColumnSetup(newMaxOutput + 2);
    ensureDynamicPowerColumn(newMaxOutput);
    fillDynamicMetaRows(newMaxOutput);
    fillDynamicDataRows(newMaxOutput, vm->dynamicRows().size());

    syncDynamicTable();
}

void Page2::ensureT1T2ColumnSetup(int t1t2Col)
{
    tblDynamic->horizontalHeader()->setSectionResizeMode(t1t2Col, QHeaderView::Fixed);
    tblDynamic->setColumnWidth(t1t2Col, 80);

    for (int row = 0; row < kMetaRowsDynamic; ++row) {
        if (QWidget *widget = tblDynamic->cellWidget(row, t1t2Col)) {
            tblDynamic->removeCellWidget(row, t1t2Col);
            widget->deleteLater();
        }

        QTableWidgetItem *item = tblDynamic->item(row, t1t2Col);
        if (!item) {
            item = new QTableWidgetItem;
            tblDynamic->setItem(row, t1t2Col, item);
        }
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        item->setText("");
        item->setBackground(QBrush(QColor(240, 240, 240)));
    }
}

void Page2::setupTableHeaders(TableKind kind, const QStringList &headers)
{
    auto *tbl = tableByKind(kind);
    QStringList fullHeaders;
    fullHeaders << "Seq";
    fullHeaders << headers;
    tbl->setColumnCount(fullHeaders.size());
    tbl->setHorizontalHeaderLabels(fullHeaders);

    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tbl->setColumnWidth(0, 36);

    // DC Table: col 1 (DC Input) 固定寬度, col 2 (Vin) Stretch
    if (kind == TableKind::Dc) {
        tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        tbl->setColumnWidth(1, 120);
        tbl->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    } else {
        tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        tbl->setColumnWidth(1, 160);
    }

    for (int col = 2; col < tbl->columnCount(); ++col) {
        tbl->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
        tbl->setColumnWidth(col, 80);
    }
}

void Page2::handleRelayHeaders(int maxOutput)
{
    const QSignalBlocker blocker(tblRelay);

    ensureRelayMetaRows(maxOutput);
    QStringList tags(maxOutput, "d");
    extendRows(TableKind::Relay, tags);
}

void Page2::handleLoadHeaders()
{
    const QSignalBlocker blocker(tblLoad);
    resetLoadTable();
}

void Page2::onRowAddRequested(TableKind kind, const QStringList &validatorTags)
{
    if (kind == TableKind::Load && tblLoad->rowCount() == 0) {
        ensureLoadMetaRows(validatorTags.size());
        ensurePowerColumn(validatorTags.size());
    }

    if (kind == TableKind::Relay && tblRelay->rowCount() == 0) {
        ensureRelayMetaRows(validatorTags.size());
    }

    createRowWidgets(tableByKind(kind), validatorTags);
    syncUIToViewModel();
    if (kind == TableKind::DyLoad)
        vm->broadcastAllDynamicPowers();
}

void Page2::onRowRemoveRequested(TableKind kind)
{
    auto *tbl = tableByKind(kind);
    const int minRows = (kind == TableKind::Load) ? kMetaRowsLoad : (kind == TableKind::DyLoad) ? kMetaRowsDynamic : 0;

    if (tbl->rowCount() > minRows) {
        tbl->removeRow(tbl->rowCount() - 1);
        refreshSeqCol(tbl);
    }

    syncUIToViewModel();
    if (kind == TableKind::DyLoad)
        vm->broadcastAllDynamicPowers();
}

void Page2::onInputTitleChanged(int row, const QString & /*dummy*/)
{
    InputRow inputRow;

    if (auto *cb = qobject_cast<QComboBox*>(tblInput->cellWidget(row, 2)))
        inputRow.phaseMode = cb->currentText();
    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 3)))
        inputRow.vin = le->text();
    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 4)))
        inputRow.frequency = le->text();
    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 5)))
        inputRow.phase = le->text();

    tblInput->item(row, 1)->setText(Page2ViewModel::inputTitle(inputRow));
}

void Page2::onPowerUpdated(int row, double value)
{
    const int powerCol = tblLoad->columnCount() - 1;
    QTableWidgetItem *item = tblLoad->item(row, powerCol);

    if (!item) {
        item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        tblLoad->setItem(row, powerCol, item);
    }

    if (std::isnan(value))
        item->setText("");
    else
        item->setText(QString::number(value, 'f', 3));
}

void Page2::onDynamicPowerUpdated(int row, double value)
{
    const int powerCol = tblDynamic->columnCount() - 1;
    QTableWidgetItem *item = tblDynamic->item(row, powerCol);

    if (!item) {
        item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        tblDynamic->setItem(row, powerCol, item);
    }

    if (std::isnan(value))
        item->setText("");
    else
        item->setText(QString::number(value, 'f', 3));
}

void Page2::resetUIFromViewModel()
{
    resetInputTable();
    resetDcTable();
    resetRelayTable();
    resetLoadTable();
    resetDynamicTable();
}

// ========== 重置輔助函數 ==========

void Page2::resetInputTable()
{
    for (int row = 0; row < tblInput->rowCount(); ++row) {
        for (int col = 0; col < tblInput->columnCount(); ++col) {
            if (QWidget* widget = tblInput->cellWidget(row, col)) {
                tblInput->removeCellWidget(row, col);
                delete widget;
            }
        }
    }

    int numRows = vm->inputRows().size();
    tblInput->setRowCount(numRows);
    tblInput->setColumnCount(6);
    tblInput->setHorizontalHeaderLabels({"Seq", "AC Input", "Mode", "Vin", "Frequency", "Phase"});

    for (int row = 0; row < numRows; ++row) {
        const auto& inputRow = vm->inputRows()[row];

        auto *seqItem = new QTableWidgetItem(QString::number(row + 1));
        seqItem->setTextAlignment(Qt::AlignCenter);
        seqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tblInput->setItem(row, 0, seqItem);

        QString label = Page2ViewModel::inputTitle(inputRow);
        auto *item = new QTableWidgetItem(label);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tblInput->setItem(row, 1, item);

        QComboBox *cbMode = makeInputPhaseComboBox(tblInput, row, 2, this, vm);
        { QSignalBlocker block(cbMode); cbMode->setCurrentText(inputRow.phaseMode.isEmpty() ? QStringLiteral("1phase") : inputRow.phaseMode); }

        QLineEdit *leVin  = makeLineEdit(tblInput, row, 3, 'd', this, vm, TableKind::Input);
        { QSignalBlocker block(leVin);  leVin->setText(inputRow.vin); }

        QLineEdit *leFreq = makeLineEdit(tblInput, row, 4, 'd', this, vm, TableKind::Input);
        { QSignalBlocker block(leFreq); leFreq->setText(inputRow.frequency); }

        QLineEdit *lePhase = makeLineEdit(tblInput, row, 5, 'd', this, vm, TableKind::Input);
        { QSignalBlocker block(lePhase); lePhase->setText(inputRow.phase); }
    }
    refreshSeqCol(tblInput);
}

// 欄位：col 0 = Seq (item, non-editable), col 1 = Vin (QLineEdit, 'd' validator)
void Page2::resetDcTable()
{
    // 清除舊 widgets
    for (int row = 0; row < tblDc->rowCount(); ++row) {
        for (int col = 0; col < tblDc->columnCount(); ++col) {
            if (QWidget* w = tblDc->cellWidget(row, col)) {
                tblDc->removeCellWidget(row, col);
                delete w;
            }
        }
    }

    const int numRows = vm->dcRows().size();
    tblDc->setRowCount(numRows);
    tblDc->setColumnCount(3);
    tblDc->setHorizontalHeaderLabels({"Seq", "DC Input", "Vin"});

    for (int row = 0; row < numRows; ++row) {
        const auto& dcRow = vm->dcRows()[row];

        // col 0: Seq
        auto *seqItem = new QTableWidgetItem(QString::number(row + 1));
        seqItem->setTextAlignment(Qt::AlignCenter);
        seqItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tblDc->setItem(row, 0, seqItem);

        // col 1: DC Input 顯示欄（顯示 Vin 值，不可編輯）
        auto *inputItem = new QTableWidgetItem(dcRow.vin);
        inputItem->setTextAlignment(Qt::AlignCenter);
        inputItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        tblDc->setItem(row, 1, inputItem);

        // col 2: Vin 輸入欄
        QLineEdit *leVin = makeLineEdit(tblDc, row, 2, 'd', this, vm, TableKind::Dc);
        connect(leVin, &QLineEdit::textChanged, this, [=](const QString& text) {
            if (auto *item = tblDc->item(row, 1))
                item->setText(text);
        });
        { QSignalBlocker block(leVin); leVin->setText(dcRow.vin); }
    }
    refreshSeqCol(tblDc);
}

void Page2::resetRelayTable()
{
    int maxRelayOutput = vm->maxRelayOutput();
    int relayDataRows  = vm->relayRows().size();

    setupRelayTableStructure(maxRelayOutput, relayDataRows);
    fillRelayDataRows(maxRelayOutput, relayDataRows);
    refreshSeqCol(tblRelay);
}

void Page2::setupRelayTableStructure(int maxRelayOutput, int relayDataRows)
{
    tblRelay->setRowCount(kMetaRowsRelay + relayDataRows);
    tblRelay->setColumnCount(maxRelayOutput + 2);

    QStringList relayHeaders{"Seq", "Relay"};
    for (int i = 1; i <= maxRelayOutput; ++i)
        relayHeaders << QString("Index%1").arg(i);
    tblRelay->setHorizontalHeaderLabels(relayHeaders);

    ensureRelayMetaRows(maxRelayOutput);
}

void Page2::fillRelayDataRows(int maxRelayOutput, int relayDataRows)
{
    for (int i = 0; i < relayDataRows; ++i) {
        int row = kMetaRowsRelay + i;
        const auto& dataRow = vm->relayRows()[i];

        fillRelayDataLabel(row, dataRow.label);
        fillRelayDataComboBoxes(row, maxRelayOutput, dataRow.values);
    }
}

void Page2::fillRelayDataLabel(int row, const QString& label)
{
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblRelay->cellWidget(row, 1));
    if (!leLabel) {
        leLabel = makeLineEdit(tblRelay, row, 1, QChar(), this, vm, TableKind::Relay);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(TableKind::Relay, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillRelayDataComboBoxes(int row, int maxRelayOutput, const QVector<QString>& values)
{
    for (int col = 0; col < maxRelayOutput; ++col) {
        QComboBox* cb = qobject_cast<QComboBox*>(tblRelay->cellWidget(row, col + 2));
        if (!cb)
            cb = makeComboBox(tblRelay, row, col + 2, this, vm, TableKind::Relay);

        QSignalBlocker block(cb);
        QString value = col < values.size() ? values[col] : "off";
        cb->setCurrentText(value.isEmpty() ? "off" : value);
    }
}

void Page2::resetLoadTable()
{
    tblLoad->clearContents();
    tblLoad->setRowCount(0);
    tblLoad->setColumnCount(0);

    int maxOutput = vm->loadMeta().names.size();
    int metaRows  = kMetaRowsLoad;
    int dataRows  = vm->loadRows().size();

    setupLoadTableStructure(maxOutput, metaRows, dataRows);
    fillLoadMetaRows(maxOutput, metaRows);
    fillLoadDataRows(maxOutput, metaRows, dataRows);
    refreshSeqCol(tblLoad);
}

void Page2::setupLoadTableStructure(int maxOutput, int metaRows, int dataRows)
{
    tblLoad->setRowCount(metaRows + dataRows);
    tblLoad->setColumnCount(maxOutput + 3);

    QStringList headers{"Seq", "Output"};
    for (int i = 1; i <= maxOutput; ++i)
        headers << QString("Index%1").arg(i);
    headers << "Power";
    tblLoad->setHorizontalHeaderLabels(headers);

    ensureLoadMetaRows(maxOutput);
    ensurePowerColumn(maxOutput);
}

void Page2::fillLoadMetaRows(int maxOutput, int metaRows)
{
    const auto& meta = vm->loadMeta();
    auto metaRowVals = std::vector<QVector<QString>>{
        meta.modes, meta.ranges, meta.names, meta.vo, meta.von,
    };

    for (int row = 0; row < metaRows; ++row) {
        for (int col = 0; col < maxOutput; ++col) {
            if (row == 0) {
                fillLoadModeCell(row, col, meta.modes);
            } else if (row == 1) {
                fillLoadRangeCell(row, col, meta.ranges);
            } else {
                fillLoadMetaCell(row, col, metaRowVals[row]);
            }
        }
    }
}

void Page2::fillLoadModeCell(int row, int col, const QVector<QString>& modes)
{
    QComboBox* cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(row, col + 2));
    if (!cb) {
        cb = new QComboBox(tblLoad);
        cb->addItems({"CC", "CV"});
        tblLoad->setCellWidget(row, col + 2, cb);
        connect(cb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                this, [=] {
                    this->syncUIToViewModel();
                    vm->cellValueChanged(TableKind::Load, row, col + 1, cb->currentText());
                    vm->broadcastAllPowers();
                });
    } else {
        for (const auto& mode : {"CC", "CV"}) {
            if (cb->findText(mode) < 0)
                cb->addItem(mode);
        }
    }
    QSignalBlocker block(cb);
    cb->setCurrentText(col < modes.size() ? modes[col] : "CC");
}

void Page2::fillLoadRangeCell(int row, int col, const QVector<QString>& ranges)
{
    QComboBox* cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(row, col + 2));
    QComboBox* modeCb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col + 2));
    const QString baseMode = modeCb ? modeCb->currentText() : QStringLiteral("CC");
    const QStringList rangeOptions = vm->loadRangeOptions(col + 1, baseMode);

    if (!cb) {
        cb = new QComboBox(tblLoad);
        cb->addItems(rangeOptions);
        tblLoad->setCellWidget(row, col + 2, cb);
        connect(cb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                this, [=] {
                    this->syncUIToViewModel();
                    vm->cellValueChanged(TableKind::Load, row, col + 1, cb->currentText());
                });
    } else {
    }
    cb->installEventFilter(this);
    StyleUtils::applyComboBoxStyle(cb, true);
    QSignalBlocker block(cb);
    const QString selected = col < ranges.size() ? ranges[col].trimmed() : QString();
    cb->clear();
    cb->addItems(rangeOptions);
    cb->setCurrentText(rangeOptions.contains(selected) ? selected : QStringLiteral("Auto Range"));
    StyleUtils::applyComboBoxStyle(cb, true);
}

void Page2::updateLoadRangeCellOptions(int outputIndex)
{
    if (outputIndex <= 0)
        return;

    const int col = outputIndex + 1;
    QComboBox* rangeCb = qobject_cast<QComboBox*>(tblLoad->cellWidget(1, col));
    QComboBox* modeCb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col));
    if (!rangeCb)
        return;

    const QString current = rangeCb->currentText().trimmed();
    const QString baseMode = modeCb ? modeCb->currentText() : QStringLiteral("CC");
    const QStringList options = vm->loadRangeOptions(outputIndex, baseMode);

    QSignalBlocker block(rangeCb);
    rangeCb->clear();
    rangeCb->addItems(options);
    rangeCb->setCurrentText(options.contains(current) ? current : QStringLiteral("Auto Range"));
    StyleUtils::applyComboBoxStyle(rangeCb, true);
}

void Page2::fillLoadMetaCell(int row, int col, const QVector<QString>& values)
{
    QLineEdit* le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, col + 2));
    if (!le) {
        le = makeLineEdit(tblLoad, row, col + 2,
                          (row == 2) ? QChar() : QChar('d'),
                          this, vm, TableKind::Load);
    }
    QSignalBlocker block(le);
    le->setText(col < values.size() ? values[col] : "");
}

void Page2::fillLoadDataRows(int maxOutput, int metaRows, int dataRows)
{
    for (int i = 0; i < dataRows; ++i) {
        int row = metaRows + i;
        const auto& dataRow = vm->loadRows()[i];

        fillLoadDataLabel(row, dataRow.label);
        fillLoadDataValues(row, maxOutput, dataRow.values);
        fillLoadPowerCell(row, maxOutput, i);
    }
}

void Page2::fillLoadDataLabel(int row, const QString& label)
{
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, 1));
    if (!leLabel) {
        leLabel = makeLineEdit(tblLoad, row, 1, QChar(), this, vm, TableKind::Load);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(TableKind::Load, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillLoadDataValues(int row, int maxOutput, const QVector<QString>& values)
{
    for (int col = 0; col < maxOutput; ++col) {
        QLineEdit* le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, col + 2));
        if (!le) {
            le = makeLineEdit(tblLoad, row, col + 2, 'l', this, vm, TableKind::Load);
        } else {
            le->setValidator(makeLoadValueVal(le));
        }

        QSignalBlocker block(le);
        le->setText(col < values.size() ? values[col] : "");
    }
}

void Page2::fillLoadPowerCell(int row, int maxOutput, int dataRowIndex)
{
    double power = vm->calcRowPower(dataRowIndex);
    int powerCol = maxOutput + 2;

    QTableWidgetItem* item = tblLoad->item(row, powerCol);
    if (!item) {
        item = new QTableWidgetItem();
        tblLoad->setItem(row, powerCol, item);
    }

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setTextAlignment(Qt::AlignCenter);

    if (std::isnan(power))
        item->setText("");
    else
        item->setText(QString::number(power, 'f', 3));
}

void Page2::resetDynamicTable()
{
    int dMaxOutput = std::max({int(vm->dynamicMeta().ranges.size()),
                               int(vm->dynamicMeta().vo.size()),
                               int(vm->dynamicMeta().von.size())});
    int dDataRows  = vm->dynamicRows().size();

    setupDynamicTableStructure(dMaxOutput, dDataRows);
    fillDynamicMetaRows(dMaxOutput);
    fillDynamicDataRows(dMaxOutput, dDataRows);
    refreshSeqCol(tblDynamic);
}

void Page2::setupDynamicTableStructure(int dMaxOutput, int dDataRows)
{
    int dMetaRows = kMetaRowsDynamic;
    tblDynamic->setRowCount(dMetaRows + dDataRows);
    tblDynamic->setColumnCount(dMaxOutput + 4);

    QStringList headers{"Seq", "Output"};
    for (int i = 1; i <= dMaxOutput; ++i)
        headers << QString("Index%1").arg(i);
    headers << "T1~T2 (ms)" << "Power";
    tblDynamic->setHorizontalHeaderLabels(headers);

    ensureDynamicMetaRows(dMaxOutput);

    int t1t2Col = dMaxOutput + 2;
    ensureT1T2ColumnSetup(t1t2Col);
    ensureDynamicPowerColumn(dMaxOutput);
}

void Page2::fillDynamicMetaRows(int dMaxOutput)
{
    int dMetaRows = kMetaRowsDynamic;
    const auto& dmeta = vm->dynamicMeta();

    auto dmetaRowVals = std::vector<QVector<QString>>{
        dmeta.ranges, dmeta.vo, dmeta.von
    };

    for (int row = 0; row < dMetaRows; ++row) {
        for (int col = 0; col < dMaxOutput; ++col) {
            if (row == 0)
                fillDynamicRangeCell(row, col, dmeta.ranges);
            else
                fillDynamicMetaCell(row, col, dmetaRowVals[row]);
        }
    }
}

void Page2::ensureDynamicPowerColumn(int maxOutput)
{
    const int powerCol = maxOutput + 3;
    const int expectedCols = powerCol + 1;

    if (tblDynamic->columnCount() != expectedCols)
        tblDynamic->setColumnCount(expectedCols);

    tblDynamic->setHorizontalHeaderItem(powerCol, new QTableWidgetItem("Power"));
    tblDynamic->horizontalHeader()->setSectionResizeMode(powerCol, QHeaderView::Fixed);
    tblDynamic->setColumnWidth(powerCol, 80);

    for (int row = 0; row < tblDynamic->rowCount(); ++row) {
        if (QWidget *widget = tblDynamic->cellWidget(row, powerCol)) {
            tblDynamic->removeCellWidget(row, powerCol);
            widget->deleteLater();
        }

        QTableWidgetItem *item = tblDynamic->item(row, powerCol);
        if (!item) {
            item = new QTableWidgetItem;
            tblDynamic->setItem(row, powerCol, item);
        }
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        item->setText("");
        if (row < kMetaRowsDynamic)
            item->setBackground(QBrush(QColor(240, 240, 240)));
    }
}

void Page2::fillDynamicRangeCell(int row, int col, const QVector<QString>& ranges)
{
    QComboBox* cb = qobject_cast<QComboBox*>(tblDynamic->cellWidget(row, col + 2));
    const QStringList rangeOptions = vm->dynamicRangeOptions(col + 1);

    if (!cb) {
        cb = new QComboBox(tblDynamic);
        cb->addItems(rangeOptions);
        tblDynamic->setCellWidget(row, col + 2, cb);
        connect(cb, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
                this, [=] {
                    this->syncUIToViewModel();
                    vm->cellValueChanged(TableKind::DyLoad, row, col + 1, cb->currentText());
                });
    }

    cb->installEventFilter(this);
    StyleUtils::applyComboBoxStyle(cb, true);
    QSignalBlocker block(cb);
    const QString selected = col < ranges.size() ? ranges[col].trimmed() : QString();
    cb->clear();
    cb->addItems(rangeOptions);
    cb->setCurrentText(rangeOptions.contains(selected) ? selected : QStringLiteral("Auto Range"));
    StyleUtils::applyComboBoxStyle(cb, true);
}

void Page2::fillDynamicMetaCell(int row, int col, const QVector<QString>& values)
{
    QLineEdit* le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col + 2));
    if (!le) {
        le = makeLineEdit(tblDynamic, row, col + 2, 'd', this, vm, TableKind::DyLoad);
    }
    QSignalBlocker block(le);
    le->setText(col < values.size() ? values[col] : "");
}

void Page2::fillDynamicDataRows(int dMaxOutput, int dDataRows)
{
    int dMetaRows = kMetaRowsDynamic;
    const auto& t1t2Data = vm->dynamicMeta().t1t2;
    const auto& dataRows = vm->dynamicRows();

    for (int i = 0; i < dDataRows && i < dataRows.size(); ++i) {
        int row = dMetaRows + i;
        const auto& dataRow = dataRows[i];

        fillDynamicDataLabel(row, dataRow.label);
        fillDynamicDataValues(row, dMaxOutput, dataRow.values);

        int t1t2Col = dMaxOutput + 2;
        QLineEdit* leT1T2 = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, t1t2Col));
        if (!leT1T2)
            leT1T2 = makeLineEdit(tblDynamic, row, t1t2Col, 'r', this, vm, TableKind::DyLoad);

        QSignalBlocker block(leT1T2);
        leT1T2->setText(i < t1t2Data.size() ? t1t2Data[i] : "");

        fillDynamicPowerCell(row, dMaxOutput, i);
    }
}

void Page2::fillDynamicDataLabel(int row, const QString& label)
{
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, 1));
    if (!leLabel) {
        leLabel = makeLineEdit(tblDynamic, row, 1, QChar(), this, vm, TableKind::DyLoad);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(TableKind::DyLoad, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillDynamicDataValues(int row, int dMaxOutput, const QVector<QString>& values)
{
    for (int col = 0; col < dMaxOutput; ++col) {
        QLineEdit* le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col + 2));
        if (!le)
            le = makeLineEdit(tblDynamic, row, col + 2, 'r', this, vm, TableKind::DyLoad);

        QSignalBlocker block(le);
        le->setText(col < values.size() ? values[col] : "");
    }
}

void Page2::fillDynamicPowerCell(int row, int dMaxOutput, int dataRowIndex)
{
    double power = vm->calcDynamicRowPower(dataRowIndex);
    int powerCol = dMaxOutput + 3;

    QTableWidgetItem* item = tblDynamic->item(row, powerCol);
    if (!item) {
        item = new QTableWidgetItem();
        tblDynamic->setItem(row, powerCol, item);
    }

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setTextAlignment(Qt::AlignCenter);

    if (std::isnan(power))
        item->setText("");
    else
        item->setText(QString::number(power, 'f', 3));
}

// ========== 初始化方法 ==========

void Page2::initializeUi()
{
    btnAddInput   = new QPushButton("+", this);
    btnSubInput   = new QPushButton("-", this);
    btnAddDc      = new QPushButton("+", this);
    btnSubDc      = new QPushButton("-", this);
    btnAddRelay   = new QPushButton("+", this);
    btnSubRelay   = new QPushButton("-", this);
    btnAddLoad    = new QPushButton("+", this);
    btnSubLoad    = new QPushButton("-", this);
    btnAddDynamic = new QPushButton("+", this);
    btnSubDynamic = new QPushButton("-", this);

    tblInput   = new QTableWidget(this);
    tblDc      = new QTableWidget(this);
    tblRelay   = new QTableWidget(this);
    tblLoad    = new QTableWidget(this);
    tblDynamic = new QTableWidget(this);

    StyleUtils::applyTableStyle(tblInput);
    StyleUtils::applyTableStyle(tblDc);
    StyleUtils::applyTableStyle(tblRelay);
    StyleUtils::applyTableStyle(tblLoad);
    StyleUtils::applyTableStyle(tblDynamic);
}

void Page2::setupLayouts()
{
    auto *h1 = new QHBoxLayout;
    h1->addWidget(btnAddInput);
    h1->addWidget(btnSubInput);

    auto *hDc = new QHBoxLayout;
    hDc->addWidget(btnAddDc);
    hDc->addWidget(btnSubDc);

    auto *h2 = new QHBoxLayout;
    h2->addWidget(btnAddRelay);
    h2->addWidget(btnSubRelay);

    auto *h3 = new QHBoxLayout;
    h3->addWidget(btnAddLoad);
    h3->addWidget(btnSubLoad);

    auto *h4 = new QHBoxLayout;
    h4->addWidget(btnAddDynamic);
    h4->addWidget(btnSubDynamic);

    auto *vLeft = new QVBoxLayout;
    vLeft->addLayout(h1);
    vLeft->addWidget(tblInput);
    vLeft->addLayout(hDc);
    vLeft->addWidget(tblDc);
    vLeft->addLayout(h2);
    vLeft->addWidget(tblRelay);

    auto *vLoad = new QVBoxLayout;
    vLoad->addLayout(h3);
    vLoad->addWidget(tblLoad);

    auto *vDynamic = new QVBoxLayout;
    vDynamic->addLayout(h4);
    vDynamic->addWidget(tblDynamic);

    auto *vRight = new QVBoxLayout;
    vRight->addLayout(vLoad);
    vRight->addLayout(vDynamic);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(vLeft, 1);
    mainLayout->addLayout(vRight, 3);
    setLayout(mainLayout);
}

void Page2::setupConnections()
{
    connect(vm, &Page2ViewModel::headersChanged,     this, &Page2::onHeadersChanged);
    connect(vm, &Page2ViewModel::rowAddRequested,    this, &Page2::onRowAddRequested);
    connect(vm, &Page2ViewModel::rowRemoveRequested, this, &Page2::onRowRemoveRequested);
    connect(vm, &Page2ViewModel::inputTitleChanged,  this, &Page2::onInputTitleChanged);
    connect(vm, &Page2ViewModel::powerUpdated,       this, &Page2::onPowerUpdated);
    connect(vm, &Page2ViewModel::dynamicPowerUpdated, this, &Page2::onDynamicPowerUpdated);
    connect(vm, &Page2ViewModel::dataChanged,        this, &Page2::resetUIFromViewModel);

    connect(this, &Page2::inputRowsChanged,   vm, &Page2ViewModel::onInputRowsChanged);
    connect(this, &Page2::dcRowsChanged,      vm, &Page2ViewModel::onDcRowsChanged);
    connect(this, &Page2::relayRowsChanged,   vm, &Page2ViewModel::onRelayRowsChanged);
    connect(this, &Page2::loadMetaChanged,    vm, &Page2ViewModel::onLoadMetaChanged);
    connect(this, &Page2::loadRowsChanged,    vm, &Page2ViewModel::onLoadRowsChanged);
    connect(this, &Page2::dynamicMetaChanged, vm, &Page2ViewModel::onDynamicMetaChanged);
    connect(this, &Page2::dynamicRowsChanged, vm, &Page2ViewModel::onDynamicRowsChanged);

    connectTableItemChanged(tblRelay,   TableKind::Relay,  kMetaRowsRelay);
    connectTableItemChanged(tblLoad,    TableKind::Load,   kMetaRowsLoad);
    connectTableItemChanged(tblDynamic, TableKind::DyLoad, kMetaRowsDynamic);
    // DC table 無 QTableWidgetItem 型 label，不需要 itemChanged 連接

    connectButtonToViewModel(btnAddInput,   TableKind::Input,  true);
    connectButtonToViewModel(btnSubInput,   TableKind::Input,  false);
    connectButtonToViewModel(btnAddDc,      TableKind::Dc,     true);
    connectButtonToViewModel(btnSubDc,      TableKind::Dc,     false);
    connectButtonToViewModel(btnAddRelay,   TableKind::Relay,  true);
    connectButtonToViewModel(btnSubRelay,   TableKind::Relay,  false);
    connectButtonToViewModel(btnAddLoad,    TableKind::Load,   true);
    connectButtonToViewModel(btnSubLoad,    TableKind::Load,   false);
    connectButtonToViewModel(btnAddDynamic, TableKind::DyLoad, true);
    connectButtonToViewModel(btnSubDynamic, TableKind::DyLoad, false);
}

void Page2::connectTableItemChanged(QTableWidget* tbl, TableKind kind, int metaRows)
{
    connect(tbl, &QTableWidget::itemChanged, this, [=](QTableWidgetItem* item) {
        int row = item->row();
        int col = item->column();
        if (col == 1 && row >= metaRows) {
            syncUIToViewModel();
            vm->cellValueChanged(kind, row, 0, item->text());
        }
    });
}

void Page2::connectButtonToViewModel(QPushButton* btn, TableKind kind, bool isAdd)
{
    connect(btn, &QPushButton::clicked, vm, [=] {
        isAdd ? vm->addRow(kind) : vm->removeRow(kind);
    });
}

void Page2::setupDelegates()
{
    auto* delegate = new NavDelegate(this, this);
    tblInput->setItemDelegate(delegate);
    tblDc->setItemDelegate(delegate);
    tblRelay->setItemDelegate(delegate);
    tblLoad->setItemDelegate(delegate);
    tblDynamic->setItemDelegate(delegate);

    tblRelay->setEditTriggers(QAbstractItemView::AllEditTriggers);
    tblLoad->setEditTriggers(QAbstractItemView::AllEditTriggers);
    tblDynamic->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

void Page2::setupInitialTableState()
{
    // DC table: 3 欄（Seq + DC Input + Vin）
    tblDc->setColumnCount(3);
    tblDc->setHorizontalHeaderLabels({"Seq", "DC Input", "Vin"});
    tblDc->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tblDc->setColumnWidth(0, 36);
    tblDc->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    tblDc->setColumnWidth(1, 120);
    tblDc->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    tblInput->setColumnCount(6);
    tblInput->setHorizontalHeaderLabels({"Seq", "AC Input", "Mode", "Vin", "Frequency", "Phase"});
    tblInput->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 所有表格通用設定
    for (auto *tbl : {tblInput, tblDc, tblRelay, tblLoad, tblDynamic}) {
        tbl->verticalHeader()->setVisible(false);
        tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
        tbl->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tbl->setContextMenuPolicy(Qt::CustomContextMenu);
        tbl->installEventFilter(this);

        connect(tbl, &QTableWidget::customContextMenuRequested,
                this, [this, tbl](const QPoint& pos) {
                    showTableContextMenu(tbl, pos);
                });

        auto *copyShortcut = new QShortcut(QKeySequence::Copy, tbl,
                                           nullptr, nullptr, Qt::WidgetShortcut);
        connect(copyShortcut, &QShortcut::activated, this, [this, tbl]() {
            copySelectedDataRows(tbl);
        });

        auto *pasteShortcut = new QShortcut(QKeySequence::Paste, tbl,
                                            nullptr, nullptr, Qt::WidgetShortcut);
        connect(pasteShortcut, &QShortcut::activated, this, [this, tbl]() {
            const auto idxList = tbl->selectionModel()->selectedRows();
            int insertAfter = -1;
            for (const auto &idx : idxList)
                insertAfter = qMax(insertAfter, idx.row());
            pasteDataRows(tbl, insertAfter);
        });

        connect(tbl->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, [this, tbl]() { updateTableSelectionVisuals(tbl); });
    }

    int initOutputs = 1;
    vm->setMaxOutput(initOutputs);
    vm->setMaxRelayOutput(initOutputs);

    vm->addRow(TableKind::Dc);
    vm->addRow(TableKind::Relay);
    vm->addRow(TableKind::Load);
    vm->addRow(TableKind::DyLoad);
}


// =========================================================
//  Seq 欄相關輔助函數
// =========================================================

int Page2::metaRowsOf(const QTableWidget* tbl) const
{
    if (tbl == tblLoad)    return kMetaRowsLoad;
    if (tbl == tblDynamic) return kMetaRowsDynamic;
    return 0;  // Input / Dc / Relay = 0
}

void Page2::refreshSeqCol(QTableWidget* tbl)
{
    static const QStringList loadLabels = {"Mode", "Range", "Name", "Vo", "Von"};
    static const QStringList dynLabels  = {"Range", "Vo", "Von"};

    const int meta = metaRowsOf(tbl);
    const int rows = tbl->rowCount();

    tbl->blockSignals(true);
    for (int r = 0; r < rows; ++r) {
        QTableWidgetItem *item0 = tbl->item(r, 0);
        if (!item0) {
            item0 = new QTableWidgetItem;
            item0->setTextAlignment(Qt::AlignCenter);
            item0->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            tbl->setItem(r, 0, item0);
        }

        if (r < meta) {
            item0->setText("");
            item0->setBackground(QBrush(QColor(225, 232, 245)));
            item0->setForeground(QBrush(QColor(70, 90, 140)));

            QTableWidgetItem *item1 = tbl->item(r, 1);
            if (!item1) {
                item1 = new QTableWidgetItem;
                item1->setTextAlignment(Qt::AlignCenter);
                item1->setFlags(Qt::ItemIsEnabled);
                tbl->setItem(r, 1, item1);
            }

            QString label;
            if (tbl == tblLoad    && r < loadLabels.size()) label = loadLabels[r];
            else if (tbl == tblDynamic && r < dynLabels.size())  label = dynLabels[r];

            item1->setText(label);
            item1->setBackground(QBrush(QColor(225, 232, 245)));

            QFont f = tbl->font();
            f.setBold(true);
            item0->setFont(f);
            item1->setFont(f);

        } else {
            // Data rows: 1, 2, 3...
            item0->setText(QString::number(r - meta + 1));
            QFont f = tbl->font();
            f.setBold(false);
            item0->setFont(f);
        }
    }
    tbl->blockSignals(false);

    updateTableSelectionVisuals(tbl);
}

QStringList Page2::tagsForKind(TableKind kind) const
{
    switch (kind) {
    case TableKind::Input:
        return {"phaseMode", "double", "double", "double"};
    case TableKind::Dc:
        return {"d"};
    case TableKind::Relay: {
        QStringList tags;
        for (int i = 0; i < vm->maxRelayOutput(); ++i) tags << "combo";
        return tags;
    }
    case TableKind::Load: {
        QStringList tags;
        for (int i = 0; i < vm->maxOutput(); ++i) tags << "l";
        return tags;
    }
    case TableKind::DyLoad: {
        QStringList tags;
        for (int i = 0; i < vm->maxOutput(); ++i) tags << "r";
        return tags;
    }
    }
    return {};
}

QString Page2::readCellText(QTableWidget* tbl, int row, int col)
{
    if (auto *cb = qobject_cast<QComboBox*>(tbl->cellWidget(row, col)))
        return cb->currentText();
    if (auto *le = qobject_cast<QLineEdit*>(tbl->cellWidget(row, col)))
        return le->text();
    if (auto *item = tbl->item(row, col))
        return item->text();
    return {};
}

void Page2::writeCellText(QTableWidget* tbl, int row, int col, const QString& text)
{
    if (auto *cb = qobject_cast<QComboBox*>(tbl->cellWidget(row, col))) {
        QSignalBlocker b(cb); cb->setCurrentText(text);
    } else if (auto *le = qobject_cast<QLineEdit*>(tbl->cellWidget(row, col))) {
        QSignalBlocker b(le); le->setText(text);
    } else if (auto *item = tbl->item(row, col)) {
        item->setText(text);
    }
}

// =========================================================
//  右鍵選單
// =========================================================
void Page2::showTableContextMenu(QTableWidget* tbl, const QPoint& viewportPos)
{
    const int meta = metaRowsOf(tbl);
    const auto idxList = tbl->selectionModel()->selectedRows();
    QList<int> selRows;
    for (const auto &idx : idxList)
        if (idx.row() >= meta) selRows.append(idx.row());
    std::sort(selRows.begin(), selRows.end());

    const int clickedRow = tbl->rowAt(viewportPos.y());
    const bool hasSelection = !selRows.isEmpty();
    const bool hasClipboard = !m_clipboard.rows.isEmpty() && m_clipboard.kind == kindOf(tbl);

    static const QString menuStyle = R"(
        QMenu {
            background: #ffffff; border: 1px solid #c8d0e0;
            border-radius: 4px; padding: 4px 0px; font-size: 12px;
        }
        QMenu::item { padding: 6px 28px 6px 14px; color: #2c3e50; }
        QMenu::item:selected { background: #e8f0fe; color: #1a56db; }
        QMenu::item:disabled { color: #bbb; }
        QMenu::separator { height: 1px; background: #e0e5ef; margin: 3px 8px; }
    )";

    QMenu menu(this);
    menu.setStyleSheet(menuStyle);

    const QString copyLabel = hasSelection
                                  ? QString("📋  Copy  (%1 row%2)  Ctrl+C").arg(selRows.size()).arg(selRows.size() > 1 ? "s" : "")
                                  : "📋  Copy  Ctrl+C";
    auto *copyAct = menu.addAction(copyLabel);
    copyAct->setEnabled(hasSelection);
    connect(copyAct, &QAction::triggered, this, [this, tbl]() { copySelectedDataRows(tbl); });

    const QString pasteLabel = hasClipboard
                                   ? QString("📌  Paste  (%1 row%2)  Ctrl+V").arg(m_clipboard.rows.size()).arg(m_clipboard.rows.size() > 1 ? "s" : "")
                                   : "📌  Paste  (clipboard empty)  Ctrl+V";
    auto *pasteAct = menu.addAction(pasteLabel);
    pasteAct->setEnabled(hasClipboard);
    connect(pasteAct, &QAction::triggered, this, [this, tbl, clickedRow]() {
        pasteDataRows(tbl, clickedRow >= metaRowsOf(tbl) ? clickedRow : tbl->rowCount() - 1);
    });

    menu.addSeparator();

    const QString delLabel = hasSelection
                                 ? QString("🗑  Delete  (%1 row%2)").arg(selRows.size()).arg(selRows.size() > 1 ? "s" : "")
                                 : "🗑  Delete";
    auto *delAct = menu.addAction(delLabel);
    delAct->setEnabled(hasSelection);
    connect(delAct, &QAction::triggered, this, [this, tbl]() { deleteSelectedDataRows(tbl); });

    menu.exec(tbl->viewport()->mapToGlobal(viewportPos));
}

void Page2::copySelectedDataRows(QTableWidget* tbl)
{
    const int meta = metaRowsOf(tbl);
    const auto idxList = tbl->selectionModel()->selectedRows();
    if (idxList.isEmpty()) return;

    QList<int> rows;
    for (const auto &idx : idxList)
        if (idx.row() >= meta) rows.append(idx.row());
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) return;

    m_clipboard.kind = kindOf(tbl);
    m_clipboard.rows.clear();

    const int cols = tbl->columnCount();
    for (int r : rows) {
        QVector<QString> rowData;
        rowData.reserve(cols);
        for (int c = 0; c < cols; ++c)
            rowData << readCellText(tbl, r, c);
        m_clipboard.rows << rowData;
    }
}

void Page2::pasteDataRows(QTableWidget* tbl, int insertAfterRow)
{
    if (m_clipboard.rows.isEmpty()) return;
    if (m_clipboard.kind != kindOf(tbl)) return;

    const int meta   = metaRowsOf(tbl);
    const int cols   = tbl->columnCount();
    QStringList tags = tagsForKind(kindOf(tbl));

    int insertAt = qMax(insertAfterRow + 1, meta);

    tbl->blockSignals(true);
    tbl->clearSelection();

    for (const auto& clipRow : m_clipboard.rows) {
        tbl->insertRow(insertAt);
        createRowWidgetsAt(tbl, insertAt, tags);

        for (int c = 1; c < cols && c < clipRow.size(); ++c) {
            if (tbl == tblLoad && c == cols - 1) continue;
            if (tbl == tblDynamic && c == cols - 1) continue;
            writeCellText(tbl, insertAt, c, clipRow[c]);
        }
        tbl->selectRow(insertAt);
        ++insertAt;
    }

    tbl->blockSignals(false);
    refreshSeqCol(tbl);
    syncUIToViewModel();
    if (tbl == tblLoad) vm->broadcastAllPowers();
    if (tbl == tblDynamic) vm->broadcastAllDynamicPowers();
}

void Page2::deleteSelectedDataRows(QTableWidget* tbl)
{
    const int meta = metaRowsOf(tbl);
    const auto idxList = tbl->selectionModel()->selectedRows();
    if (idxList.isEmpty()) return;

    QList<int> rows;
    for (const auto &idx : idxList)
        if (idx.row() >= meta) rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    if (rows.isEmpty()) return;

    tbl->blockSignals(true);
    for (int r : rows) tbl->removeRow(r);
    tbl->blockSignals(false);

    tbl->clearSelection();
    refreshSeqCol(tbl);
    syncUIToViewModel();
    if (tbl == tblDynamic)
        vm->broadcastAllDynamicPowers();
}

void Page2::updateTableSelectionVisuals(QTableWidget* tbl)
{
    const int meta = metaRowsOf(tbl);
    const int rows = tbl->rowCount();
    const int cols = tbl->columnCount();
    static const QColor kBg("#3399ff");

    const auto selIdxes = tbl->selectionModel()->selectedRows();
    auto isSel = [&](int r) {
        for (const auto& idx : selIdxes) if (idx.row() == r) return true;
        return false;
    };

    for (int r = meta; r < rows; ++r) {
        const bool sel = isSel(r);
        for (int c = 0; c < cols; ++c) {
            QWidget* w = tbl->cellWidget(r, c);
            if (!w) continue;
            if (auto* le = qobject_cast<QLineEdit*>(w)) {
                sel ? le->setStyleSheet(QString("QLineEdit{background:%1;color:white;border:none;}").arg(kBg.name()))
                    : StyleUtils::applyLineEditStyle(le);
            } else if (auto* cb = qobject_cast<QComboBox*>(w)) {
                sel ? cb->setStyleSheet(QString("QComboBox{background:%1;color:white;border:none;}"
                                               "QComboBox::drop-down{border:none;}"
                                               "QComboBox QAbstractItemView{background:white;color:black;}").arg(kBg.name()))
                    : StyleUtils::applyComboBoxStyle(cb, true);
            }
        }
    }
}
