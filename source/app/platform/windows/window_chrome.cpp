#include "window_chrome.h"
#include <QGuiApplication>
#include <QToolButton>
#include <QStyle>
#include <QWidget>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif
bool configureWindowFrame(QWidget *window) {
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != "windows") return true;
    const auto handle = reinterpret_cast<HWND>(window->winId());
    auto style = GetWindowLongPtrW(handle,GWL_STYLE);
    style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetLastError(0);
    if (!SetWindowLongPtrW(handle,GWL_STYLE,style) && GetLastError()) return false;
    const BOOL dark = FALSE;
    DwmSetWindowAttribute(handle,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    const auto corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(handle,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
    const COLORREF border = RGB(221,215,233);
    DwmSetWindowAttribute(handle,DWMWA_BORDER_COLOR,&border,sizeof(border));
    const MARGINS margins{1,1,1,1};
    DwmExtendFrameIntoClientArea(handle,&margins);
    return SetWindowPos(handle,nullptr,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    Q_UNUSED(window);
    return true;
#endif
}
bool handleWindowFrame(QWidget *window, QWidget *titleBar, QToolButton *maximize, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != "windows" || !titleBar || !maximize) return false;
    const auto *event = static_cast<MSG *>(message);
    if (event->message == WM_NCCALCSIZE && event->wParam) {
        // The window still owns native frame styles. Only the client-area calculation is customized.
        if (IsZoomed(event->hwnd)) {
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(MonitorFromWindow(event->hwnd,MONITOR_DEFAULTTONEAREST),&monitor))
                reinterpret_cast<NCCALCSIZE_PARAMS *>(event->lParam)->rgrc[0] = monitor.rcWork;
        }
        *result = 0;
        return true;
    }
    if (event->message == WM_NCHITTEST) {
        const POINT point{GET_X_LPARAM(event->lParam),GET_Y_LPARAM(event->lParam)};
        RECT bounds{};
        GetWindowRect(event->hwnd,&bounds);
        if (!IsZoomed(event->hwnd)) {
            const auto dpi = GetDpiForWindow(event->hwnd);
            const int edge = GetSystemMetricsForDpi(SM_CXSIZEFRAME,dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi);
            const bool left = point.x < bounds.left + edge;
            const bool right = point.x >= bounds.right - edge;
            const bool top = point.y < bounds.top + edge;
            const bool bottom = point.y >= bounds.bottom - edge;
            if (top || bottom || left || right) {
                if (top) *result = left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
                else if (bottom) *result = left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
                else *result = left ? HTLEFT : HTRIGHT;
                return true;
            }
        }
        auto client = point;
        ScreenToClient(event->hwnd,&client);
        const auto scale = window->devicePixelRatioF();
        const QPoint position(qRound(client.x/scale),qRound(client.y/scale));
        const QPoint barPosition = titleBar->mapFrom(window,position);
        if (titleBar->rect().contains(barPosition)) {
            auto *child = titleBar->childAt(barPosition);
            if (child == maximize) *result = HTMAXBUTTON;
            else if (qobject_cast<QAbstractButton *>(child)) *result = HTCLIENT;
            else *result = HTCAPTION;
            return true;
        }
    }
    if (event->message == WM_NCLBUTTONDOWN && event->wParam == HTMAXBUTTON) {
        maximize->setDown(true);
        SetCapture(event->hwnd);
        *result = 0;
        return true;
    }
    if ((event->message == WM_LBUTTONUP || event->message == WM_NCLBUTTONUP) && maximize->isDown()) {
        POINT point{GET_X_LPARAM(event->lParam),GET_Y_LPARAM(event->lParam)};
        if (event->message == WM_NCLBUTTONUP) ScreenToClient(event->hwnd,&point);
        const auto scale = window->devicePixelRatioF();
        const QPoint local(qRound(point.x/scale),qRound(point.y/scale));
        const bool inside = maximize->rect().contains(maximize->mapFrom(window,local));
        maximize->setDown(false);
        ReleaseCapture();
        if (inside) maximize->click();
        *result = 0;
        return true;
    }
    if (event->message == WM_CAPTURECHANGED || event->message == WM_CANCELMODE) maximize->setDown(false);
    if (event->message == WM_NCMOUSEMOVE || event->message == WM_NCMOUSELEAVE || event->message == WM_MOUSEMOVE) {
        const bool hover = event->message == WM_NCMOUSEMOVE && event->wParam == HTMAXBUTTON;
        if (hover) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT),TME_LEAVE | TME_NONCLIENT,event->hwnd,0};
            TrackMouseEvent(&tracking);
        }
        if (maximize->property("nativeHover").toBool() != hover) {
            maximize->setProperty("nativeHover",hover);
            maximize->style()->unpolish(maximize);
            maximize->style()->polish(maximize);
        }
    }
#else
    Q_UNUSED(window); Q_UNUSED(titleBar); Q_UNUSED(maximize); Q_UNUSED(message); Q_UNUSED(result);
#endif
    return false;
}
