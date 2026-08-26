#pragma once

// ══════════════════════════════════════════════════════
//  Page5 統一樣式配置
//  所有顏色、字型、樣式都集中在這裡管理
// ══════════════════════════════════════════════════════

namespace Page5Style {

constexpr auto MAIN = R"(
    QWidget {
        background-color: #f0f0f0;
        font-family: "Segoe UI";
        font-size: 12px;
    }
)";

constexpr auto TABLE = R"(
    QTableWidget {
        background-color: white;
        gridline-color: #cccccc;
        selection-background-color: #3399ff;
        selection-color: white;
        border: 1px solid #aaaaaa;
    }
    QTableWidget::item { padding: 2px 4px; }
    QHeaderView::section {
        background-color: #dce6f1;
        color: #333333;
        border: 1px solid #aaaaaa;
        padding: 3px;
        font-weight: bold;
    }
)";

constexpr auto TAB = R"(
    QTabWidget::pane {
        border: 1px solid #aaaaaa;
        background-color: white;
    }
    QTabBar::tab {
        background: #dce6f1;
        border: 1px solid #aaaaaa;
        padding: 5px 14px;
        font-weight: bold;
    }
    QTabBar::tab:selected {
        background: white;
        border-bottom: none;
        color: #003399;
    }
)";

constexpr auto SPLITTER = R"(
    QSplitter::handle { background-color: #b0b8c8; }
    QSplitter::handle:hover { background-color: #3399ff; }
)";

constexpr auto PANEL_TITLE = R"(
    QLabel {
        background-color: #4472c4;
        color: white;
        font-weight: bold;
        font-size: 13px;
        padding: 5px 8px;
    }
)";

//QToolButton:hover 當滑鼠游標懸停在元件上方時的樣式。
constexpr auto SIDE_TOGGLE_BTN = R"(
    QToolButton {
        background-color: #c8d0e0;
        border: none;
    }
    QToolButton:hover {
        background-color: #3399ff;
        color: white;
    }
)";

constexpr auto OUTPUT_WINDOW = R"(
    QTextEdit {
        background-color: white;
        border: 1px solid #aaa;
        font-family: "Consolas", monospace;
        font-size: 11px;
    }
)";

} // namespace Page5Style
