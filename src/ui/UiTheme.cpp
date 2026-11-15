#include "ui/UiTheme.h"

namespace pbl {
namespace ui {

QString build_dark_stylesheet() {
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background-color: #0b0f19;
            color: #f1f5f9;
            font-family: "Segoe UI", "Inter", "Roboto", sans-serif;
            font-size: 12px;
        }
        QGroupBox {
            background-color: #111827;
            border: 1px solid #1f293d;
            border-radius: 8px;
            margin-top: 14px;
            padding-top: 12px;
            font-weight: bold;
            font-size: 11px;
            color: #94a3b8;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            background-color: #111827;
            color: #38bdf8;
            font-weight: bold;
        }
        QTabWidget::pane {
            border: 1px solid #1f293d;
            background-color: #111827;
            border-radius: 8px;
            top: -1px;
        }
        QTabBar {
            background-color: #0b0f19;
            qproperty-drawBase: 0;
        }
        QTabBar::tab {
            background-color: #1e293b;
            color: #94a3b8;
            border: 1px solid #1f293d;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 8px 14px;
            margin-right: 3px;
            font-weight: 600;
        }
        QTabBar::tab:hover {
            background-color: #334155;
            color: #f1f5f9;
        }
        QTabBar::tab:selected {
            background-color: #111827;
            color: #38bdf8;
            border-bottom: 2px solid #38bdf8;
            font-weight: bold;
        }
        QPushButton {
            background-color: #1e293b;
            color: #f1f5f9;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #334155;
            border-color: #475569;
        }
        QPushButton:pressed {
            background-color: #0f172a;
        }
        QPushButton:disabled {
            background-color: #1e293b;
            color: #64748b;
            border-color: #1e293b;
        }
        QLineEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #0b0f19;
            color: #f1f5f9;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 5px 8px;
            selection-background-color: #2563eb;
        }
        QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #38bdf8;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: 1px solid #334155;
        }
        QComboBox QAbstractItemView {
            background-color: #0b0f19;
            color: #f1f5f9;
            selection-background-color: #1e3a5f;
            selection-color: #38bdf8;
            border: 1px solid #334155;
        }
        QTableWidget {
            background-color: #0b0f19;
            alternate-background-color: #111827;
            color: #f1f5f9;
            gridline-color: #1f293d;
            border: 1px solid #1f293d;
            border-radius: 6px;
            selection-background-color: #1e3a5f;
            selection-color: #38bdf8;
        }
        QHeaderView::section {
            background-color: #172033;
            color: #94a3b8;
            font-weight: bold;
            font-size: 11px;
            border: none;
            border-bottom: 2px solid #2563eb;
            padding: 6px;
        }
        QTableCornerButton::section {
            background-color: #172033;
            border: none;
            border-bottom: 2px solid #2563eb;
        }
        QAbstractScrollArea::corner {
            background-color: #0b0f19;
            border: none;
        }
        QProgressBar {
            background-color: #0b0f19;
            border: 1px solid #334155;
            border-radius: 6px;
            text-align: center;
            color: #ffffff;
            font-weight: bold;
            height: 18px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #06b6d4);
            border-radius: 5px;
        }
        QSplitter::handle {
            background-color: #1f293d;
        }
        QSplitter::handle:hover {
            background-color: #38bdf8;
        }
        QScrollBar:vertical {
            background-color: #0b0f19;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: #334155;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #475569;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: #0b0f19;
        }
        QScrollBar:horizontal {
            background-color: #0b0f19;
            height: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background-color: #334155;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #475569;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollArea, QWidget#qt_scrollarea_viewport {
            background-color: #111827;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background-color: #111827;
        }
        QMenu {
            background-color: #111827;
            color: #f1f5f9;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 18px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #1e3a5f;
            color: #38bdf8;
        }
        QMenu::separator {
            height: 1px;
            background: #1f293d;
            margin: 4px 8px;
        }
        QMessageBox {
            background-color: #111827;
            color: #f1f5f9;
        }
        QMessageBox QLabel {
            background-color: transparent;
            color: #f1f5f9;
            font-size: 12px;
        }
        QMessageBox QPushButton {
            min-width: 75px;
            padding: 6px 14px;
        }
        QToolTip {
            background-color: #1e293b;
            color: #f8fafc;
            border: 1px solid #38bdf8;
            padding: 6px;
            border-radius: 4px;
        }
    )");
}

}  // namespace ui
}  // namespace pbl
