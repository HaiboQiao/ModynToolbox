#include "theme.h"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
void Theme::apply() {
    if (qApp->property("modynThemeApplied").toBool())
        return;
    qApp->setProperty("modynThemeApplied", true);
    qApp->setStyle(QStyleFactory::create("Fusion"));
    auto palette = qApp->palette();
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, panel);
    palette.setColor(QPalette::AlternateBase, background);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, panel);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, panel);
    qApp->setPalette(palette);
    auto style = QStringLiteral(R"(
        QWidget { color:@text; font-family:'Microsoft YaHei UI','Segoe UI'; font-size:14px; }
        QMainWindow, QWidget#workbenchRoot { background:@background; }
        QWidget#titleBar { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #fcfbff,stop:1 @navigation); border-bottom:1px solid @border; }
        QLabel#appTitle { font-size:14px; font-weight:600; }
        QLabel#caption, QLabel#description, QLabel#secondary, QLabel#catalogStatus { color:@muted; font-size:12px; }
        QWidget#sidebar { background:@navigation; border-right:1px solid @border; }
        QListWidget#toolNavigation { background:transparent; border:0; outline:0; }
        QListWidget#toolNavigation::item { min-height:24px; padding:10px 8px; margin:2px 0; border-radius:8px; }
        QListWidget#toolNavigation::item:hover { background:#eae5f6; }
        QListWidget#toolNavigation::item:selected { background:@selection; color:#5438b6; }
        QLabel#heading { font-size:23px; font-weight:600; }
        QWidget#toolPanel { background:@panel; border:1px solid @border; border-radius:12px; }
        QLineEdit,QPlainTextEdit,QComboBox,QSpinBox { background:@panel; border:1px solid @border; border-radius:7px; padding:8px; selection-background-color:@accent; }
        QComboBox::drop-down { width:26px; border:0; }
        QPlainTextEdit { font-family:Consolas,'Microsoft YaHei UI'; font-size:13px; }
        QLineEdit:focus,QPlainTextEdit:focus,QComboBox:focus,QSpinBox:focus { border-color:@accent; }
        QPushButton { background:@panel; border:1px solid @border; border-radius:7px; padding:8px 12px; }
        QPushButton:hover { background:#f3effd; border-color:#bbaedc; }
        QPushButton:focus { border-color:@accent; }
        QPushButton[primary="true"] { background:@accent; border-color:@accent; color:@panel; }
        QPushButton[primary="true"]:hover { background:#6044c9; }
        QPushButton:disabled { background:#efedf4; color:#777185; border-color:@border; }
        QToolButton#windowControl { background:transparent; border:0; border-radius:4px; }
        QToolButton#windowControl:hover { background:#e5dff4; }
        QToolButton#windowClose { background:transparent; border:0; border-radius:4px; }
        QToolButton#windowClose:hover { background:#f5ccd4; }
        QToolButton[nativeHover="true"] { background:#e5dff4; }
        QSlider::groove:horizontal { height:5px; background:#e5dfef; border-radius:2px; }
        QSlider::sub-page:horizontal { background:@accent; border-radius:2px; }
        QSlider::handle:horizontal { background:@accent; width:14px; margin:-5px 0; border-radius:7px; }
        QFrame#fileDropArea { background:#f8f6ff; border:1px dashed #bfb0e7; border-radius:9px; }
        QTableWidget { background:@panel; border:0; gridline-color:#eeebf5; selection-background-color:@selection; selection-color:@text; }
        QHeaderView::section { background:#f7f5fb; color:@muted; border:0; padding:10px 8px; text-align:left; font-size:12px; }
        QTableWidget::item { padding:7px; border-bottom:1px solid #eeebf5; }
        QScrollArea { background:transparent; border:0; }
        QScrollBar:vertical { background:transparent; width:9px; margin:0; }
        QScrollBar::handle:vertical { background:#c9c2da; border-radius:4px; min-height:30px; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
        QToolTip { background:@panel; color:@text; border:1px solid @border; padding:6px; }
    )");
    style.replace("@background", background.name())
        .replace("@navigation", navigation.name())
        .replace("@selection", selection.name())
        .replace("@panel", panel.name())
        .replace("@muted", muted.name())
        .replace("@border", border.name())
        .replace("@accent", accent.name())
        .replace("@text", text.name());
    const int size = qBound(
        12, qApp->property("modynFontSize").toInt() ? qApp->property("modynFontSize").toInt() : 14,
        18);
    style.replace("font-size:14px;", "font-size:@body;")
        .replace("font-size:12px;", "font-size:@secondary;")
        .replace("font-size:13px;", "font-size:@mono;")
        .replace("font-size:23px;", "font-size:@heading;");
    style.replace("@body", QString("%1px").arg(size))
        .replace("@secondary", QString("%1px").arg(size - 2))
        .replace("@mono", QString("%1px").arg(size - 1))
        .replace("@heading", QString("%1px").arg(size + 9));
    if (qApp->property("modynCompact").toBool())
        style.replace("padding:8px 12px;", "padding:5px 10px;")
            .replace("padding:10px 8px;", "padding:6px 8px;");
    qApp->setStyleSheet(style);
}

void Theme::setPreferences(int fontSize, bool compact) {
    qApp->setProperty("modynFontSize", qBound(12, fontSize, 18));
    qApp->setProperty("modynCompact", compact);
    qApp->setProperty("modynThemeApplied", false);
    apply();
}
