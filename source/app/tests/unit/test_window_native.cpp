#include "ui/window/chrome_window.h"
#include "ui/theme/theme.h"
#include <QApplication>
#include <QLayout>
#include <QToolButton>
#include <QtTest>
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
class NativeFrameTests : public QObject {
    Q_OBJECT
private slots:
    void hiddenFrameStylesAndHitRegions() {
        Theme::apply();
        ChromeWindow window;
        window.resize(1000,700);
        window.setContentWidget(new QWidget);
        window.ensurePolished();
        window.centralWidget()->resize(window.size());
        window.centralWidget()->layout()->activate();
        window.titleBar()->layout()->activate();
        window.initializeNativeFrame();
        const auto handle=reinterpret_cast<HWND>(window.winId());
        QVERIFY(IsWindow(handle));
        QVERIFY(!IsWindowVisible(handle));
        const auto style=GetWindowLongPtrW(handle,GWL_STYLE);
        QVERIFY(style & WS_THICKFRAME);
        QVERIFY(style & WS_MAXIMIZEBOX);
        RECT bounds{};
        QVERIFY(GetWindowRect(handle,&bounds));
        QCOMPARE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(bounds.left+1,bounds.top+1)),LRESULT(HTTOPLEFT));
        QCOMPARE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(bounds.right-1,bounds.bottom-1)),LRESULT(HTBOTTOMRIGHT));
        POINT caption{30,27};
        ClientToScreen(handle,&caption);
        const auto scale=window.devicePixelRatioF();
        caption.x=qRound((caption.x-bounds.left)*scale)+bounds.left;
        caption.y=qRound((caption.y-bounds.top)*scale)+bounds.top;
        QCOMPARE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(caption.x,caption.y)),LRESULT(HTCAPTION));
        const auto button=window.maximizeButton()->mapTo(&window,window.maximizeButton()->rect().center());
        POINT maximum{qRound(button.x()*scale),qRound(button.y()*scale)};
        ClientToScreen(handle,&maximum);
        QCOMPARE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(maximum.x,maximum.y)),LRESULT(HTMAXBUTTON));
        auto *close=window.findChild<QToolButton *>("windowClose");
        const auto closePosition=close->mapTo(&window,close->rect().center());
        POINT closePoint{qRound(closePosition.x()*scale),qRound(closePosition.y()*scale)};
        ClientToScreen(handle,&closePoint);
        QCOMPARE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(closePoint.x,closePoint.y)),LRESULT(HTCLIENT));
        QVERIFY(!IsWindowVisible(handle));
    }
    void shownFrameKeepsEightResizeEdges() {
        Theme::apply();
        ChromeWindow window;
        window.setWindowFlag(Qt::Tool);
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        auto *content = new QWidget;
        content->setAttribute(Qt::WA_NativeWindow);
        window.setContentWidget(content);
        window.setMinimumSize(620,460);
        window.resize(1000,700);
        window.move(-20000,-20000);
        window.show();
        QTest::qWait(30);
        const auto hwnd = reinterpret_cast<HWND>(window.winId());
        QVERIFY(IsWindowVisible(hwnd));
        QVERIFY(GetWindowLongPtrW(hwnd,GWL_STYLE) & WS_THICKFRAME);
        RECT b{}; QVERIFY(GetWindowRect(hwnd,&b));
        const int x=(b.left+b.right)/2,y=(b.top+b.bottom)/2;
        const struct {int x,y;LRESULT hit;} points[] = {
            {b.left+1,y,HTLEFT},{b.right-1,y,HTRIGHT},{x,b.top+1,HTTOP},{x,b.bottom-1,HTBOTTOM},
            {b.left+1,b.top+1,HTTOPLEFT},{b.right-1,b.top+1,HTTOPRIGHT},
            {b.left+1,b.bottom-1,HTBOTTOMLEFT},{b.right-1,b.bottom-1,HTBOTTOMRIGHT}};
        for (const auto &point:points)
            QCOMPARE(SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(point.x,point.y)),point.hit);
        // Ask the actual top native child at the pointer location, not just the main HWND.
        for(const auto &point:points) {
            POINT local{point.x,point.y};ScreenToClient(hwnd,&local);
            const auto child=ChildWindowFromPointEx(hwnd,local,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED|CWP_SKIPTRANSPARENT);
            QVERIFY(child && child!=hwnd);
            QCOMPARE(SendMessageW(child,WM_NCHITTEST,0,MAKELPARAM(point.x,point.y)),point.hit);
            COLORREF key{};BYTE alpha{};DWORD flags{};
            auto *grip=QWidget::find(WId(child));QVERIFY(grip);QCOMPARE(grip->objectName(),QString("windowResizeHandle"));QVERIFY(grip->property("nativeLayerReady").toBool());
            const bool attributes=GetLayeredWindowAttributes(child,&key,&alpha,&flags);
            QVERIFY2(attributes,qPrintable(QString("layer attributes error=%1 exstyle=%2").arg(GetLastError()).arg(qulonglong(GetWindowLongPtrW(child,GWL_EXSTYLE)))));QCOMPARE(alpha,BYTE(1));
        }
        QCOMPARE(content->mapTo(&window,QPoint()).x(),0);
        QCOMPARE(content->mapTo(&window,content->rect().bottomRight()).x(),window.width()-1);
        window.resize(850,550); QCoreApplication::processEvents();
        QCOMPARE(window.size(),QSize(850,550));
        window.resize(100,100); QCoreApplication::processEvents();
        QCOMPARE(window.size(),window.minimumSize());
        window.hide();
    }
    void maximizeCaptureCanBeCancelled() {
        Theme::apply();
        ChromeWindow window;
        window.setContentWidget(new QWidget);
        window.initializeNativeFrame();
        const auto handle=reinterpret_cast<HWND>(window.winId());
        window.maximizeButton()->setDown(true);
        QVERIFY(window.maximizeButton()->isDown());
        SendMessageW(handle,WM_CANCELMODE,0,0);
        QVERIFY(!window.maximizeButton()->isDown());
        QVERIFY(!IsWindowVisible(handle));
    }
};
QTEST_MAIN(NativeFrameTests)
#include "test_window_native.moc"
