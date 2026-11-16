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
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
namespace {
class ResizeHandle final : public QWidget {
    QWidget *owner;
    int hit;
public:
    ResizeHandle(QWidget *window,int region):QWidget(window),owner(window),hit(region) {
        setObjectName("windowResizeHandle");setProperty("nativeHit",hit);
        setAttribute(Qt::WA_NativeWindow);setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);setAutoFillBackground(false);
        auto colors=palette();colors.setColor(QPalette::Window,Qt::black);setPalette(colors);
        setCursor(hit==10||hit==11?Qt::SizeHorCursor:hit==12||hit==15?Qt::SizeVerCursor:
                  hit==13||hit==17?Qt::SizeFDiagCursor:Qt::SizeBDiagCursor);
    }
protected:
    void paintEvent(QPaintEvent *) override {QPainter painter(this);painter.fillRect(rect(),QColor(0,0,0,1));}
    bool nativeEvent(const QByteArray &type,void *message,qintptr *result) override {
        if(handleResizeHandle(owner,hit,message,result))return true;
        return QWidget::nativeEvent(type,message,result);
    }
};
class CloseButton final : public QToolButton {
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        const bool highlighted=isEnabled()&&(underMouse()||isDown());
        if(highlighted)painter.fillRect(rect(),isDown()?Theme::closePressed:Theme::closeHover);
        painter.setPen(QPen(highlighted?Theme::panel:Theme::muted,1.3));
        const QPointF c=QRectF(rect()).center();const qreal radius=4;
        painter.drawLine(c+QPointF(-radius,-radius),c+QPointF(radius,radius));
        painter.drawLine(c+QPointF(-radius,radius),c+QPointF(radius,-radius));
        if(hasFocus()){painter.setPen(QPen(highlighted?Theme::panel:Theme::accent,1,Qt::DotLine));painter.drawRect(rect().adjusted(3,3,-4,-4));}
    }
};
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
    frame->setObjectName("windowFrame");
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
        QToolButton *button = icon==UiIcon::Close ? new CloseButton : new QToolButton;
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
    QMainWindow::showEvent(event);
    // Qt can update native styles while showing a frameless window.
    nativeFrameReady = configureWindowFrame(this);
    syncResizeHandles();
    QTimer::singleShot(0,this,&ChromeWindow::syncResizeHandles);
}
void ChromeWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange && maximize) {
        const bool expanded = isMaximized();
        syncResizeHandles();
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

void ChromeWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);syncResizeHandles();
}
void ChromeWindow::syncResizeHandles() {
    if(QGuiApplication::platformName()!="windows"||!isVisible())return;
    if(resizeHandles.isEmpty()) {
        // Create the native content ancestor before its sibling input handles so it cannot cover them.
        frame->winId();
        for(int hit=10;hit<=17;++hit)resizeHandles.append(new ResizeHandle(this,hit));
    }
    const int n=8,w=width(),h=height();
    const QRect regions[]={QRect(0,n,n,h-2*n),QRect(w-n,n,n,h-2*n),QRect(n,0,w-2*n,n),
        QRect(0,0,n,n),QRect(w-n,0,n,n),QRect(n,h-n,w-2*n,n),QRect(0,h-n,n,n),QRect(w-n,h-n,n,n)};
    for(int i=0;i<resizeHandles.size();++i) {
        auto *handle=resizeHandles[i];handle->setGeometry(regions[i]);
        handle->setVisible(!isMaximized()&&!isFullScreen());
        if(handle->isVisible())configureResizeHandle(handle);
    }
}
