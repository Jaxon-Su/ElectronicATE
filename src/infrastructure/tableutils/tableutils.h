#pragma once
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>
#include <QString>

// ══════════════════════════════════════════════════════
//  TableUtils — 共用表格工具函式
//  所有 Page 皆可 include 使用，無需重複實作
// ══════════════════════════════════════════════════════
namespace TableUtils
{
// 建立置中 item
QTableWidgetItem* makeCenteredItem(const QString& text);

// 建立灰底粗字 meta item（表頭資訊列）
QTableWidgetItem* makeMetaItem(const QString& text);

// 建立置中 Checkbox（QPushButton 模擬）
QWidget* makeCenteredCheckbox(bool checked);

// 依列數動態調整表格高度
void fitTableHeight(QTableWidget* table, int maxH = 300);

// 套用統一表格樣式（含捲軸）
// baseStyle: 各頁面自訂的主樣式，留空則使用預設
void applyTableStyle(QTableWidget* table, const QString& baseStyle = {});
}
