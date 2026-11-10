#include "chrome_window.h"
#include "ui/components/icons.h"
#include "ui/theme/theme.h"
#include "platform/windows/window_chrome.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QShowEvent>
#include <QToolButton>
#include <QWindow>
namespace {
class TitleBar : public QWidget {
  protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && window()->windowHandle())
            window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (window()->isMaximized())
                window()->showNormal();
            else
                window()->showMaximized();
        }
    }
};
} // namespace
ChromeWindow::ChromeWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint |
                   Qt::WindowCloseButtonHint);
    setWindowTitle(QStringLiteral("ModynToolbox"));
    setWindowIcon(brandIcon());
    frame = new QWidget;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    bar = new TitleBar;
    bar->setObjectName("titleBar");
    bar->setFixedHeight(Theme::titleBarHeight);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(18, 0, 10, 0);
    row->setSpacing(10);
    auto *logo = new QLabel;
    logo->setPixmap(brandIcon().pixmap(30, 30));
    row->addWidget(logo);
    auto *name = new QLabel(QStringLiteral("ModynToolbox"));
    name->setObjectName("appTitle");
    auto *brand = new QVBoxLayout;
    brand->setSpacing(Theme::titleBrandSpacing);
    brand->setContentsMargins(0, 0, 0, 0);
    brand->addWidget(name);
    auto *caption =
        new QLabel(QStringLiteral("本地工具工作台 · v") + QStringLiteral(MODYN_VERSION));
    caption->setObjectName("caption");
    connect(this, &QWidget::windowTitleChanged, caption, [caption](const QString &title) {
        const auto prefix = QStringLiteral("ModynToolbox · ");
        caption->setText(title.startsWith(prefix) ? title.mid(prefix.size())
                                                  : QStringLiteral("本地工具工作台 · v") +
                                                        QStringLiteral(MODYN_VERSION));
    });
    brand->addWidget(caption);
    row->addLayout(brand);
    row->setAlignment(brand, Qt::AlignVCenter);
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    caption->setAttribute(Qt::WA_TransparentForMouseEvents);
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addStretch();
    auto addControl = [this, row](const QString &name, UiIcon icon) {
        auto *button = new QToolButton;
        button->setObjectName("windowControl");
        button->setAccessibleName(name);
        button->setToolTip(name);
        button->setFixedSize(38, 34);
        button->setIcon(uiIcon(icon, Theme::muted));
        row->addWidget(button);
        return button;
    };
    auto *minimize = addControl(QStringLiteral("最小化"), UiIcon::Minimize);
    minimize->setProperty("windowAction", "minimize");
    maximize = addControl(QStringLiteral("最大化"), UiIcon::Maximize);
    maximize->setProperty("windowAction", "maximize");
    auto *close = addControl(QStringLiteral("关闭窗口"), UiIcon::Close);
    close->setObjectName("windowClose");
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    layout->addWidget(bar);
    QMainWindow::setCentralWidget(frame);
}
void ChromeWindow::setContentWidget(QWidget *content) {
    frame->layout()->addWidget(content);
}
void ChromeWindow::initializeNativeFrame() {
    if (!nativeFrameReady)
        nativeFrameReady = configureWindowFrame(this);
}
void ChromeWindow::showEvent(QShowEvent *event) {
    initializeNativeFrame();
    QMainWindow::showEvent(event);
}
void ChromeWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange && maximize) {
        const bool expanded = isMaximized();
        maximize->setIcon(uiIcon(expanded ? UiIcon::Restore : UiIcon::Maximize, Theme::muted));
        const auto label = expanded ? QStringLiteral("还原窗口") : QStringLiteral("最大化");
        maximize->setAccessibleName(label);
        maximize->setToolTip(label);
    }
    QMainWindow::changeEvent(event);
}
bool ChromeWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result) {
    if (handleWindowFrame(this, bar, maximize, message, result))
        return true;
    return QMainWindow::nativeEvent(type, message, result);
}
