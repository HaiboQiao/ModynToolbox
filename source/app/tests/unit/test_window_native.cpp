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
