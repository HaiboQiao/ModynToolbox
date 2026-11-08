#include "window_theme.h"
#include <QWidget>
#include <QGuiApplication>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
void applyDarkFrame(QWidget *window) {
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != "windows") return;
    const auto handle = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const COLORREF caption = RGB(30, 33, 38);
    const COLORREF text = RGB(239, 241, 245);
    DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR, &text, sizeof(text));
#else
    Q_UNUSED(window);
#endif
}
