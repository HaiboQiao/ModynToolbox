#pragma once
#include <QtGlobal>
class QWidget;
class QToolButton;
bool configureWindowFrame(QWidget *window);
void configureResizeHandle(QWidget *handle);
bool handleResizeHandle(QWidget *window, int hit, void *message, qintptr *result);
bool handleWindowFrame(QWidget *window, QWidget *titleBar, QToolButton *maximize, void *message, qintptr *result);
