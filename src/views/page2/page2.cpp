#include "page2.h"
#include "page2viewmodel.h"
#include "styleutils.h"
#include "NavDelegate.h"
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
    syncRelayTable();
    syncLoadTable();
    syncDynamicTable();
    vm->refreshUIOutputs();
}

// ========== 表格工具函數 ==========

QTableWidget* Page2::tableByKind(LoadKind k) const
{
    switch(k) {
    case LoadKind::Input:   return tblInput;
    case LoadKind::Relay:   return tblRelay;
    case LoadKind::Load:    return tblLoad;
    case LoadKind::DyLoad:  return tblDynamic;
    }
    return nullptr;
}

LoadKind Page2::kindOf(const QTableWidget* tbl) const
{
    if (tbl == tblInput)   return LoadKind::Input;
    if (tbl == tblRelay)   return LoadKind::Relay;
    if (tbl == tblLoad)    return LoadKind::Load;
    return LoadKind::DyLoad;
}

// ========== Meta 行建立 ==========

void Page2::ensureRelayMetaRows(int maxOutput)
{
    // Relay 表格不需要 Meta 行
}

void Page2::createMetaHeaderLabel(QTableWidget* tbl, int row, const QString& text)
{
    QLabel* label = qobject_cast<QLabel*>(tbl->cellWidget(row, 0));
    if (!label) {
        label = new QLabel(text, tbl);
        label->setAlignment(Qt::AlignCenter);
        StyleUtils::applyHeaderLook(label);
        tbl->setCellWidget(row, 0, label);
    } else {
        label->setText(text);
    }
}

void Page2::ensureLoadMetaRows(int maxOutput)
{
    while (tblLoad->rowCount() < kMetaRowsLoad)
        tblLoad->insertRow(tblLoad->rowCount());

    createMetaHeaderLabel(tblLoad, 0, "Mode");
    createMetaHeaderLabel(tblLoad, 1, "Name");
    createMetaHeaderLabel(tblLoad, 2, "Vo");
    createMetaHeaderLabel(tblLoad, 3, "Von");
    createMetaHeaderLabel(tblLoad, 4, "RiseSlope(CCH)");
    createMetaHeaderLabel(tblLoad, 5, "FallSlope(CCH)");
    createMetaHeaderLabel(tblLoad, 6, "RiseSlope(CCL)");
    createMetaHeaderLabel(tblLoad, 7, "FallSlope(CCL)");

    const QStringList modes{"CC"};

    for (int col = 1; col <= maxOutput; ++col) {
        QComboBox *cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col));
        if (!cb) {
            cb = new QComboBox(tblLoad);
            cb->addItems(modes);
            tblLoad->setCellWidget(0, col, cb);
        }
        cb->installEventFilter(this);
        StyleUtils::applyComboBoxStyle(cb, true);

        for (int row = 1; row < kMetaRowsLoad; ++row) {
            if (tblLoad->cellWidget(row, col))
                continue;
            QChar tag;
            if (row == 1) {
                tag = QChar();  // Name 行無驗證器
            }
            else {
                tag = 'd';  // Vo, Von, slope行使用單一數值驗證器
            }
            makeLineEdit(tblLoad, row, col, tag, this, vm, LoadKind::Load);
        }
    }
}

void Page2::ensureDynamicMetaRows(int maxOutput)
{
    while (tblDynamic->rowCount() < kMetaRowsDynamic)
        tblDynamic->insertRow(tblDynamic->rowCount());

    createMetaHeaderLabel(tblDynamic, 0, "Vo");
    createMetaHeaderLabel(tblDynamic, 1, "Von");
    createMetaHeaderLabel(tblDynamic, 2, "RiseSlope(CCDH)");
    createMetaHeaderLabel(tblDynamic, 3, "FallSlope(CCDH)");
    createMetaHeaderLabel(tblDynamic, 4, "RiseSlope(CCDL)");
    createMetaHeaderLabel(tblDynamic, 5, "FallSlope(CCDL)");

    for (int col = 1; col <= maxOutput; ++col) {
        for (int row = 0; row < kMetaRowsDynamic; ++row) {
            if (!tblDynamic->cellWidget(row, col)) {
                QChar tag= 'd';  // Vo, Von,slope 行使用單一數值驗證器
                makeLineEdit(tblDynamic, row, col, tag, this, vm, LoadKind::DyLoad);
            }
        }
    }

    // 注意：T1~T2 欄位的設置由 ensureT1T2ColumnSetup 處理
}

void Page2::ensurePowerColumn(int maxOutput)
{
    const int powerCol = maxOutput + 1;
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
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        item->setText("");
    }
}

// ========== 行擴展與創建 ==========

void Page2::createRowWidgets(QTableWidget *tbl, const QStringList &tags)
{
    int row = tbl->rowCount();
    tbl->insertRow(row);

    auto *item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    if (tbl == tblInput)
        item->setFlags(Qt::ItemIsEnabled);
    tbl->setItem(row, 0, item);

    LoadKind kind = kindOf(tbl);

    for (int col = 1; col <= tags.size(); ++col) {
        if (tbl->cellWidget(row, col))
            continue;

        if (kind == LoadKind::Relay) {
            makeComboBox(tbl, row, col, this, vm, kind);
        } else {
            QChar tag = determineValidatorTag(tbl, row, col, tags);
            makeLineEdit(tbl, row, col, tag, this, vm, kind);
        }
    }

    // Dynamic 表格:為數據行創建 T1~T2 欄位(固定在最後一欄)
    if (tbl == tblDynamic && row >= kMetaRowsDynamic) {
        int t1t2Col = tags.size() + 1;  // T1~T2 固定位置
        if (!tbl->cellWidget(row, t1t2Col)) {
            makeLineEdit(tbl, row, t1t2Col, 'r', this, vm, LoadKind::DyLoad);
        }
    }

    if (tbl == tblLoad) {
        int powerCol = tags.size() + 1;
        if (!tbl->item(row, powerCol)) {
            auto *powerItem = new QTableWidgetItem;
            powerItem->setFlags(Qt::ItemIsEnabled);
            powerItem->setTextAlignment(Qt::AlignCenter);
            tbl->setItem(row, powerCol, powerItem);
        }
    }
}

QChar Page2::determineValidatorTag(QTableWidget* tbl, int row, int col, const QStringList& tags) const
{
    if (tbl == tblLoad && row == 1)
        return QChar();

    if (tbl == tblDynamic) {
        // Meta 行使用 'd'
        if (row < kMetaRowsDynamic)
            return 'd';

        // 數據行:最後一欄(T1~T2)使用 'r',其他使用 'r'
        int maxOutput = tags.size();
        if (col == maxOutput + 1)  // T1~T2 欄位
            return 'r';
        else if (col == 0)  // Label 欄位
            return QChar();
        else
            return 'r';  // Index 欄位
    }

    return tags[col - 1].isEmpty() ? QChar() : tags[col - 1][0];
}

void Page2::extendRows(LoadKind kind, const QStringList &tags)
{
    QTableWidget* tbl = tableByKind(kind);
    int rows = tbl->rowCount();

    for (int row = 0; row < rows; ++row) {
        if (shouldSkipMetaRow(tbl, row))
            continue;

        for (int col = 1; col <= tags.size(); ++col) {
            if (tbl->cellWidget(row, col))
                continue;

            if (kind == LoadKind::Relay) {
                makeComboBox(tbl, row, col, this, vm, kind);
            } else {
                QChar tag = tags[col - 1][0];
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
    if (tbl == tblRelay && row < kMetaRowsRelay)
        return true;
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
    static QRegularExpression regex(R"(^\d+(\.\d{0,3})?~\d+(\.\d{0,3})?$)");
    return new QRegularExpressionValidator(regex, parent);
}

QLineEdit* Page2::makeLineEdit(QTableWidget* tbl, int r, int c, QChar tag,
                               Page2* self, Page2ViewModel* vm, LoadKind kind)
{
    auto* lineEdit = new QLineEdit(tbl);
    lineEdit->setAlignment(Qt::AlignCenter);
    lineEdit->installEventFilter(self);

    if (tag == 'd')
        lineEdit->setValidator(makeDoubleVal(lineEdit, 3));
    else if (tag == 'r')
        lineEdit->setValidator(makeRangeVal(lineEdit));

    StyleUtils::applyLineEditStyle(lineEdit);
    tbl->setCellWidget(r, c, lineEdit);

    QObject::connect(lineEdit, &QLineEdit::textChanged, self, [=] {
        self->syncUIToViewModel();
        vm->cellValueChanged(kind, r, c, lineEdit->text());
        if (kind == LoadKind::Load)
            vm->broadcastAllPowers();
    });

    return lineEdit;
}

QComboBox* Page2::makeComboBox(QTableWidget* tbl, int r, int c,
                               Page2* self, Page2ViewModel* vm, LoadKind kind)
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
                         vm->cellValueChanged(kind, r, c, comboBox->currentText());
                     });

    return comboBox;
}

// ========== 同步輔助函數 ==========

void Page2::syncInputTable()
{
    QVector<InputRow> inputs;
    for (int row = 0; row < tblInput->rowCount(); ++row) {
        InputRow inputRow;
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 1)))
            inputRow.vin = le->text();
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 2)))
            inputRow.frequency = le->text();
        if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 3)))
            inputRow.phase = le->text();
        inputs.append(inputRow);
    }
    emit inputRowsChanged(inputs);
}

void Page2::syncRelayTable()
{
    QVector<RelayDataRow> relayRows;
    int maxRelayOutput = tblRelay->columnCount() - 1;

    for (int row = kMetaRowsRelay; row < tblRelay->rowCount(); ++row) {
        RelayDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblRelay->cellWidget(row, 0)))
            dataRow.label = le->text();
        else if (auto *item = tblRelay->item(row, 0))
            dataRow.label = item->text();

        for (int col = 1; col <= maxRelayOutput; ++col) {
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
    int maxOutput = tblLoad->columnCount() - 2;

    LoadMetaRow meta;
    for (int col = 1; col <= maxOutput; ++col) {
        if (auto *cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(0, col)))
            meta.modes << cb->currentText();
    }

    meta.names = extractMetaRowValues(tblLoad, 1, maxOutput);
    meta.vo = extractMetaRowValues(tblLoad, 2, maxOutput);
    meta.von = extractMetaRowValues(tblLoad, 3, maxOutput);
    meta.riseSlopeCCH = extractMetaRowValues(tblLoad, 4, maxOutput);
    meta.fallSlopeCCH = extractMetaRowValues(tblLoad, 5, maxOutput);
    meta.riseSlopeCCL = extractMetaRowValues(tblLoad, 6, maxOutput);
    meta.fallSlopeCCL = extractMetaRowValues(tblLoad, 7, maxOutput);

    emit loadMetaChanged(meta);

    QVector<LoadDataRow> loadRows;
    for (int row = kMetaRowsLoad; row < tblLoad->rowCount(); ++row) {
        LoadDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, 0)))
            dataRow.label = le->text();
        else if (auto *item = tblLoad->item(row, 0))
            dataRow.label = item->text();

        for (int col = 1; col <= maxOutput; ++col) {
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
}

void Page2::syncDynamicTable()
{
    int dMaxOutput = tblDynamic->columnCount() - 2;  // 減去 Output 和 T1~T2

    DynamicMetaRow dmeta;
    dmeta.vo = extractMetaRowValues(tblDynamic, 0, dMaxOutput);
    dmeta.von = extractMetaRowValues(tblDynamic, 1, dMaxOutput);
    dmeta.riseSlopeCCDH = extractMetaRowValues(tblDynamic, 2, dMaxOutput);
    dmeta.fallSlopeCCDH = extractMetaRowValues(tblDynamic, 3, dMaxOutput);
    dmeta.riseSlopeCCDL = extractMetaRowValues(tblDynamic, 4, dMaxOutput);
    dmeta.fallSlopeCCDL = extractMetaRowValues(tblDynamic, 5, dMaxOutput);

    // T1~T2 固定在最後一欄
    int t1t2Col = dMaxOutput + 1;
    dmeta.t1t2.clear();
    for (int row = kMetaRowsDynamic; row < tblDynamic->rowCount(); ++row) {
        if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, t1t2Col))) {
            dmeta.t1t2.append(le->text());
        } else {
            dmeta.t1t2.append(QString());
        }
    }

    emit dynamicMetaChanged(dmeta);

    QVector<DynamicDataRow> dynamicRows;
    for (int row = kMetaRowsDynamic; row < tblDynamic->rowCount(); ++row) {
        DynamicDataRow dataRow;

        if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, 0)))
            dataRow.label = le->text();
        else if (auto *item = tblDynamic->item(row, 0))
            dataRow.label = item->text();

        for (int col = 1; col <= dMaxOutput; ++col) {
            if (auto *le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col)))
                dataRow.values << le->text();
            else
                dataRow.values << QString();
        }

        dynamicRows.append(dataRow);
    }
    emit dynamicRowsChanged(dynamicRows);
}

QVector<QString> Page2::extractMetaRowValues(QTableWidget* tbl, int row, int maxOutput)
{
    QVector<QString> result;
    for (int col = 1; col <= maxOutput; ++col) {
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
        if (col < maxCol) {
            focusCell(row, col + 1);
            return true;
        }
        break;
    case Qt::Key_Left:
        if (col > 0) {
            focusCell(row, (col == 1) ? 0 : col - 1);
            return true;
        }
        break;
    case Qt::Key_Down:
        if (row < maxRow) {
            focusCell(row + 1, col);
            return true;
        }
        break;
    case Qt::Key_Up:
        if (row > 0) {
            focusCell(row - 1, col);
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

// ========== Slots ==========

void Page2::onHeadersChanged(LoadKind kind, const QStringList &headers)
{
    // 特殊處理 Dynamic 表格，避免 T1~T2 數據丟失
    if (kind == LoadKind::DyLoad) {
        handleDynamicHeadersTime(headers);
        return;
    }

    setupTableHeaders(kind, headers);

    if (kind == LoadKind::Relay) {
        handleRelayHeaders(headers.size() - 1);
    } else if (kind == LoadKind::Load) {
        handleLoadHeaders(headers.size() - 2);
    } else if (kind != LoadKind::Input) {
        QStringList tags(tableByKind(kind)->columnCount() - 1,
                         kind == LoadKind::Load ? "d" : "r");
        extendRows(kind, tags);
    }
}

void Page2::handleDynamicHeadersTime(const QStringList &headers)
{
    int newMaxOutput = headers.size() - 2;
    if (newMaxOutput < 0) return;

    syncDynamicTable();                          // 1. 保存
    setupTableHeaders(LoadKind::DyLoad, headers); // 2. 重建結構
    ensureDynamicMetaRows(newMaxOutput);         // 3. Meta 行
    ensureT1T2ColumnSetup(newMaxOutput + 1);     // 4. T1~T2 設置
    fillDynamicMetaRows(newMaxOutput);           // 5. 恢復 Meta
    fillDynamicDataRows(newMaxOutput, vm->dynamicRows().size()); // 6. 恢復數據
}

void Page2::ensureT1T2ColumnSetup(int t1t2Col)
{
    // 設置欄位寬度
    tblDynamic->horizontalHeader()->setSectionResizeMode(t1t2Col, QHeaderView::Fixed);
    tblDynamic->setColumnWidth(t1t2Col, 80);

    // 清理並設置 Meta 行的 T1~T2 欄位為不可編輯
    for (int row = 0; row < kMetaRowsDynamic; ++row) {
        // 移除任何可能存在的 Widget
        if (QWidget *widget = tblDynamic->cellWidget(row, t1t2Col)) {
            tblDynamic->removeCellWidget(row, t1t2Col);
            widget->deleteLater();
        }

        // 設置為不可編輯的空白 Item
        QTableWidgetItem *item = tblDynamic->item(row, t1t2Col);
        if (!item) {
            item = new QTableWidgetItem;
            tblDynamic->setItem(row, t1t2Col, item);
        }
        item->setFlags(Qt::ItemIsEnabled);  // 只能啟用，不能選擇或編輯
        item->setTextAlignment(Qt::AlignCenter);
        item->setText("");
        item->setBackground(QBrush(QColor(240, 240, 240)));  // 設置灰色背景以示不可編輯
    }
}

void Page2::setupTableHeaders(LoadKind kind, const QStringList &headers)
{
    auto *tbl = tableByKind(kind);
    tbl->setColumnCount(headers.size());
    tbl->setHorizontalHeaderLabels(headers);

    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tbl->setColumnWidth(0, 100);

    for (int col = 1; col < tbl->columnCount(); ++col) {
        tbl->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
        tbl->setColumnWidth(col, 80);
    }
}

void Page2::handleRelayHeaders(int maxOutput)
{
    ensureRelayMetaRows(maxOutput);
    QStringList tags(maxOutput, "d");
    extendRows(LoadKind::Relay, tags);
}

void Page2::handleLoadHeaders(int maxOutput)
{
    ensureLoadMetaRows(maxOutput);
    ensurePowerColumn(maxOutput);
    QStringList tags(maxOutput, "d");
    extendRows(LoadKind::Load, tags);
}

void Page2::handleDynamicHeaders(int maxOutput)
{
    ensureDynamicMetaRows(maxOutput);
    QStringList tags(maxOutput, "r");
    extendRows(LoadKind::DyLoad, tags);
}

void Page2::onRowAddRequested(LoadKind kind, const QStringList &validatorTags)
{
    if (kind == LoadKind::Load && tblLoad->rowCount() == 0) {
        ensureLoadMetaRows(validatorTags.size());
        ensurePowerColumn(validatorTags.size());
    }

    if (kind == LoadKind::Relay && tblRelay->rowCount() == 0) {
        ensureRelayMetaRows(validatorTags.size());
    }

    createRowWidgets(tableByKind(kind), validatorTags);
    syncUIToViewModel();
}

void Page2::onRowRemoveRequested(LoadKind kind)
{
    auto *tbl = tableByKind(kind);
    const int minRows = (kind == LoadKind::Load) ? 6 : (kind == LoadKind::DyLoad) ? 4 : 0;

    if (tbl->rowCount() > minRows)
        tbl->removeRow(tbl->rowCount() - 1);

    syncUIToViewModel();
}

void Page2::onInputTitleChanged(int row, const QString & /*dummy*/)
{
    QString vin, freq, phase;

    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 1)))
        vin = le->text();
    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 2)))
        freq = le->text();
    if (auto *le = qobject_cast<QLineEdit*>(tblInput->cellWidget(row, 3)))
        phase = le->text();

    QString title = (!vin.isEmpty() && !freq.isEmpty() && !phase.isEmpty())
                        ? QString("%1/%2/%3").arg(vin, freq, phase) : "";

    tblInput->item(row, 0)->setText(title);
    syncUIToViewModel();
}

void Page2::onMaxOutputChanged(int maxOut)
{
    vm->setMaxOutput(maxOut);
}

void Page2::onPowerUpdated(int row, double value)
{
    const int powerCol = tblLoad->columnCount() - 1;
    QTableWidgetItem *item = tblLoad->item(row, powerCol);

    if (!item) {
        item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        tblLoad->setItem(row, powerCol, item);
    }

    if (std::isnan(value))
        item->setText("");
    else
        item->setText(QString::number(value, 'f', 3));
}

void Page2::resetUIFromViewModel()
{
    resetInputTable();
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
    tblInput->setColumnCount(4);
    tblInput->setHorizontalHeaderLabels({"Input", "Vin", "Frequency", "Phase"});

    for (int row = 0; row < numRows; ++row) {
        const auto& inputRow = vm->inputRows()[row];

        QString label = (!inputRow.vin.isEmpty() && !inputRow.frequency.isEmpty() &&
                         !inputRow.phase.isEmpty())
                            ? QString("%1/%2/%3").arg(inputRow.vin, inputRow.frequency, inputRow.phase)
                            : "";

        auto *item = new QTableWidgetItem(label);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled);
        tblInput->setItem(row, 0, item);

        QLineEdit *leVin = makeLineEdit(tblInput, row, 1, 'd', this, vm, LoadKind::Input);
        { QSignalBlocker block(leVin); leVin->setText(inputRow.vin); }

        QLineEdit *leFreq = makeLineEdit(tblInput, row, 2, 'd', this, vm, LoadKind::Input);
        { QSignalBlocker block(leFreq); leFreq->setText(inputRow.frequency); }

        QLineEdit *lePhase = makeLineEdit(tblInput, row, 3, 'd', this, vm, LoadKind::Input);
        { QSignalBlocker block(lePhase); lePhase->setText(inputRow.phase); }
    }
}

void Page2::resetRelayTable()
{
    int maxRelayOutput = vm->maxRelayOutput();
    int relayDataRows = vm->relayRows().size();

    setupRelayTableStructure(maxRelayOutput, relayDataRows);
    fillRelayDataRows(maxRelayOutput, relayDataRows);
}

void Page2::setupRelayTableStructure(int maxRelayOutput, int relayDataRows)
{
    tblRelay->setRowCount(kMetaRowsRelay + relayDataRows);
    tblRelay->setColumnCount(maxRelayOutput + 1);

    QStringList relayHeaders{"Relay"};
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
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblRelay->cellWidget(row, 0));
    if (!leLabel) {
        leLabel = makeLineEdit(tblRelay, row, 0, QChar(), this, vm, LoadKind::Relay);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(LoadKind::Relay, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillRelayDataComboBoxes(int row, int maxRelayOutput, const QVector<QString>& values)
{
    for (int col = 0; col < maxRelayOutput; ++col) {
        QComboBox* cb = qobject_cast<QComboBox*>(tblRelay->cellWidget(row, col + 1));
        if (!cb)
            cb = makeComboBox(tblRelay, row, col + 1, this, vm, LoadKind::Relay);

        QSignalBlocker block(cb);
        QString value = col < values.size() ? values[col] : "off";
        cb->setCurrentText(value.isEmpty() ? "off" : value);
    }
}

void Page2::resetLoadTable()
{
    int maxOutput = vm->loadMeta().names.size();
    int metaRows = kMetaRowsLoad;
    int dataRows = vm->loadRows().size();

    setupLoadTableStructure(maxOutput, metaRows, dataRows);
    fillLoadMetaRows(maxOutput, metaRows);
    fillLoadDataRows(maxOutput, metaRows, dataRows);
}

void Page2::setupLoadTableStructure(int maxOutput, int metaRows, int dataRows)
{
    tblLoad->setRowCount(metaRows + dataRows);
    tblLoad->setColumnCount(maxOutput + 2);

    QStringList headers{"Output"};
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
        meta.modes, meta.names, meta.vo, meta.von, meta.riseSlopeCCH, meta.fallSlopeCCH, meta.riseSlopeCCL, meta.fallSlopeCCL
    };

    for (int row = 0; row < metaRows; ++row) {
        for (int col = 0; col < maxOutput; ++col) {
            if (row == 0) {
                fillLoadModeCell(row, col, meta.modes);
            } else {
                fillLoadMetaCell(row, col, metaRowVals[row]);
            }
        }
    }
}

void Page2::fillLoadModeCell(int row, int col, const QVector<QString>& modes)
{
    QComboBox* cb = qobject_cast<QComboBox*>(tblLoad->cellWidget(row, col + 1));
    if (!cb) {
        cb = new QComboBox(tblLoad);
        tblLoad->setCellWidget(row, col + 1, cb);
    }
    cb->setCurrentText(col < modes.size() ? modes[col] : "CC");
}

void Page2::fillLoadMetaCell(int row, int col, const QVector<QString>& values)
{
    QLineEdit* le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, col + 1));
    if (!le) {
        le = makeLineEdit(tblLoad, row, col + 1,
                          (row == 1) ? QChar() : QChar('d'),
                          this, vm, LoadKind::Load);
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
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, 0));
    if (!leLabel) {
        leLabel = makeLineEdit(tblLoad, row, 0, QChar(), this, vm, LoadKind::Load);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(LoadKind::Load, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillLoadDataValues(int row, int maxOutput, const QVector<QString>& values)
{
    for (int col = 0; col < maxOutput; ++col) {
        QLineEdit* le = qobject_cast<QLineEdit*>(tblLoad->cellWidget(row, col + 1));
        if (!le)
            le = makeLineEdit(tblLoad, row, col + 1, 'd', this, vm, LoadKind::Load);

        QSignalBlocker block(le);
        le->setText(col < values.size() ? values[col] : "");
    }
}

void Page2::fillLoadPowerCell(int row, int maxOutput, int dataRowIndex)
{
    double power = vm->calcRowPower(dataRowIndex);
    int powerCol = maxOutput + 1;

    QTableWidgetItem* item = tblLoad->item(row, powerCol);
    if (!item) {
        item = new QTableWidgetItem();
        tblLoad->setItem(row, powerCol, item);
    }

    item->setFlags(Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);

    if (std::isnan(power))
        item->setText("");
    else
        item->setText(QString::number(power, 'f', 3));
}

void Page2::resetDynamicTable()
{
    // 使用 vo 或 von 的大小來決定 maxOutput（與 Load 表格一致）
    int dMaxOutput = std::max(int(vm->dynamicMeta().vo.size()),
                              int(vm->dynamicMeta().von.size()));
    int dDataRows = vm->dynamicRows().size();

    setupDynamicTableStructure(dMaxOutput, dDataRows);
    fillDynamicMetaRows(dMaxOutput);
    fillDynamicDataRows(dMaxOutput, dDataRows);
}

void Page2::setupDynamicTableStructure(int dMaxOutput, int dDataRows)
{
    int dMetaRows = kMetaRowsDynamic;
    tblDynamic->setRowCount(dMetaRows + dDataRows);
    tblDynamic->setColumnCount(dMaxOutput + 2);  // +2 = Output 欄 + T1~T2 欄

    QStringList headers{"Output"};
    for (int i = 1; i <= dMaxOutput; ++i)
        headers << QString("Index%1").arg(i);
    headers << "T1~T2 (s)";
    tblDynamic->setHorizontalHeaderLabels(headers);

    ensureDynamicMetaRows(dMaxOutput);

    // 確保 T1~T2 欄位正確設置（包括 Meta 行的不可編輯狀態）
    int t1t2Col = dMaxOutput + 1;
    ensureT1T2ColumnSetup(t1t2Col);
}

void Page2::fillDynamicMetaRows(int dMaxOutput)
{
    int dMetaRows = kMetaRowsDynamic;
    const auto& dmeta = vm->dynamicMeta();
    auto dmetaRowVals = std::vector<QVector<QString>>{
        dmeta.vo, dmeta.von, dmeta.riseSlopeCCDH, dmeta.fallSlopeCCDH, dmeta.riseSlopeCCDL, dmeta.fallSlopeCCDL
    };

    for (int row = 0; row < dMetaRows; ++row) {
        for (int col = 0; col < dMaxOutput; ++col) {
            fillDynamicMetaCell(row, col, dmetaRowVals[row]);
        }
    }
}

void Page2::fillDynamicMetaCell(int row, int col, const QVector<QString>& values)
{
    QLineEdit* le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col + 1));
    if (!le) {
        QChar tag = 'd';  // Vo, Von 行使用單一數值驗證器

        le = makeLineEdit(tblDynamic, row, col + 1, tag, this, vm, LoadKind::DyLoad);
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

        int t1t2Col = dMaxOutput + 1;
        QLineEdit* leT1T2 = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, t1t2Col));
        if (!leT1T2)
            leT1T2 = makeLineEdit(tblDynamic, row, t1t2Col, 'r', this, vm, LoadKind::DyLoad);

        QSignalBlocker block(leT1T2);
        leT1T2->setText(i < t1t2Data.size() ? t1t2Data[i] : "");
    }
}

void Page2::fillDynamicDataLabel(int row, const QString& label)
{
    QLineEdit* leLabel = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, 0));
    if (!leLabel) {
        leLabel = makeLineEdit(tblDynamic, row, 0, QChar(), this, vm, LoadKind::DyLoad);
        connect(leLabel, &QLineEdit::textChanged, this, [=] {
            this->syncUIToViewModel();
            vm->cellValueChanged(LoadKind::DyLoad, row, 0, leLabel->text());
        });
    }
    QSignalBlocker block(leLabel);
    leLabel->setText(label);
}

void Page2::fillDynamicDataValues(int row, int dMaxOutput, const QVector<QString>& values)
{
    for (int col = 0; col < dMaxOutput; ++col) {
        QLineEdit* le = qobject_cast<QLineEdit*>(tblDynamic->cellWidget(row, col + 1));
        if (!le)
            le = makeLineEdit(tblDynamic, row, col + 1, 'r', this, vm, LoadKind::DyLoad);

        QSignalBlocker block(le);
        le->setText(col < values.size() ? values[col] : "");
    }
}

// ========== 初始化方法 ==========

void Page2::initializeUi()
{
    btnAddInput   = new QPushButton("+", this);
    btnSubInput   = new QPushButton("-", this);
    btnAddRelay   = new QPushButton("+", this);
    btnSubRelay   = new QPushButton("-", this);
    btnAddLoad    = new QPushButton("+", this);
    btnSubLoad    = new QPushButton("-", this);
    btnAddDynamic = new QPushButton("+", this);
    btnSubDynamic = new QPushButton("-", this);

    tblInput   = new QTableWidget(this);
    tblRelay   = new QTableWidget(this);
    tblLoad    = new QTableWidget(this);
    tblDynamic = new QTableWidget(this);

    StyleUtils::applyTableStyle(tblInput);
    StyleUtils::applyTableStyle(tblRelay);
    StyleUtils::applyTableStyle(tblLoad);
    StyleUtils::applyTableStyle(tblDynamic);
}

void Page2::setupLayouts()
{
    auto *h1 = new QHBoxLayout;
    h1->addWidget(btnAddInput);
    h1->addWidget(btnSubInput);

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
    connect(vm, &Page2ViewModel::headersChanged, this, &Page2::onHeadersChanged);
    connect(vm, &Page2ViewModel::rowAddRequested, this, &Page2::onRowAddRequested);
    connect(vm, &Page2ViewModel::rowRemoveRequested, this, &Page2::onRowRemoveRequested);
    connect(vm, &Page2ViewModel::inputTitleChanged, this, &Page2::onInputTitleChanged);
    connect(vm, &Page2ViewModel::powerUpdated, this, &Page2::onPowerUpdated);
    connect(vm, &Page2ViewModel::dataChanged, this, &Page2::resetUIFromViewModel);

    connect(this, &Page2::inputRowsChanged, vm, &Page2ViewModel::onInputRowsChanged);
    connect(this, &Page2::relayRowsChanged, vm, &Page2ViewModel::onRelayRowsChanged);
    connect(this, &Page2::loadMetaChanged, vm, &Page2ViewModel::onLoadMetaChanged);
    connect(this, &Page2::loadRowsChanged, vm, &Page2ViewModel::onLoadRowsChanged);
    connect(this, &Page2::dynamicMetaChanged, vm, &Page2ViewModel::onDynamicMetaChanged);
    connect(this, &Page2::dynamicRowsChanged, vm, &Page2ViewModel::onDynamicRowsChanged);

    connectTableItemChanged(tblRelay, LoadKind::Relay, kMetaRowsRelay);
    connectTableItemChanged(tblLoad, LoadKind::Load, kMetaRowsLoad);
    connectTableItemChanged(tblDynamic, LoadKind::DyLoad, kMetaRowsDynamic);

    connectButtonToViewModel(btnAddInput, LoadKind::Input, true);
    connectButtonToViewModel(btnSubInput, LoadKind::Input, false);
    connectButtonToViewModel(btnAddRelay, LoadKind::Relay, true);
    connectButtonToViewModel(btnSubRelay, LoadKind::Relay, false);
    connectButtonToViewModel(btnAddLoad, LoadKind::Load, true);
    connectButtonToViewModel(btnSubLoad, LoadKind::Load, false);
    connectButtonToViewModel(btnAddDynamic, LoadKind::DyLoad, true);
    connectButtonToViewModel(btnSubDynamic, LoadKind::DyLoad, false);
}

void Page2::connectTableItemChanged(QTableWidget* tbl, LoadKind kind, int metaRows)
{
    connect(tbl, &QTableWidget::itemChanged, this, [=](QTableWidgetItem* item) {
        int row = item->row();
        int col = item->column();
        if (col == 0 && row >= metaRows) {
            syncUIToViewModel();
            vm->cellValueChanged(kind, row, col, item->text());
        }
    });
}

void Page2::connectButtonToViewModel(QPushButton* btn, LoadKind kind, bool isAdd)
{
    connect(btn, &QPushButton::clicked, vm, [=] {
        isAdd ? vm->addRow(kind) : vm->removeRow(kind);
    });
}

void Page2::setupDelegates()
{
    auto* delegate = new NavDelegate(this, this);
    tblInput->setItemDelegate(delegate);
    tblRelay->setItemDelegate(delegate);
    tblLoad->setItemDelegate(delegate);
    tblDynamic->setItemDelegate(delegate);

    tblRelay->setEditTriggers(QAbstractItemView::AllEditTriggers);
    tblLoad->setEditTriggers(QAbstractItemView::AllEditTriggers);
    tblDynamic->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

void Page2::setupInitialTableState()
{
    tblInput->setColumnCount(4);
    tblInput->setHorizontalHeaderLabels({"Input", "Vin", "Frequency", "Phase"});
    tblInput->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    tblInput->verticalHeader()->setVisible(false);
    tblRelay->verticalHeader()->setVisible(false);
    tblLoad->verticalHeader()->setVisible(false);
    tblDynamic->verticalHeader()->setVisible(false);

    int initOutputs = 1;
    vm->setMaxOutput(initOutputs);
    vm->setMaxRelayOutput(initOutputs);

    vm->addRow(LoadKind::Relay);
    vm->addRow(LoadKind::Load);
    vm->addRow(LoadKind::DyLoad);
}

