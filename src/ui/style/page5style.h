#pragma once

//  Page5 統一樣式配置
//  所有顏色、字型、樣式都集中在這裡管理

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

// Page3-like action buttons, with visible pointer/keyboard feedback.
constexpr auto REPORT_ACTION_BUTTON = R"(
    QPushButton {
        font-family: "Segoe UI";
        font-size: 9pt;
        font-weight: normal;
        background: #ffffff;
        color: #111111;
        border: 1px solid #bbbbbb;
        border-radius: 4px;
        padding: 4px 12px;
        min-height: 20px;
    }
    QPushButton:enabled:focus { background: #eef5ff; border-color: #338fec; }
    QPushButton:enabled:hover { background: #e5f1ff; border-color: #338fec; }
    QPushButton:enabled:pressed { background: #86b9ec; border-color: #338fec; }
    QPushButton:disabled { background: #ededed; color: #888888; border-color: #d0d0d0; }
)";

constexpr auto REPORT_FORM = R"(
    QFrame#reportFileForm {
        background: white;
        border: 1px solid #d4dce8;
        border-radius: 6px;
    }
    QLabel { background: transparent; color: #25344a; border: none; }
    QLabel[fieldLabel="true"] { font-weight: bold; }
    QLineEdit {
        background: white;
        color: #222222;
        border: 1px solid #aebdd0;
        border-radius: 4px;
        padding: 4px 8px;
        selection-background-color: #d9e8ff;
        selection-color: #172b49;
    }
    QLineEdit:focus { border: 1px solid #4472c4; }
    QPushButton {
        font-family: "Segoe UI";
        font-size: 9pt;
        font-weight: normal;
        background: #edf3fc;
        color: #244c87;
        border: 1px solid #aebdd0;
        border-radius: 4px;
        padding: 4px 10px;
    }
    QPushButton:hover { background: #dfeaff; border-color: #4472c4; }
    QPushButton:pressed { background: #cadcf7; }
    QPushButton:focus { border: 1px solid #4472c4; }
    QLineEdit:disabled, QPushButton:disabled {
        background: #f0f2f5;
        color: #778294;
        border-color: #d4dce8;
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
