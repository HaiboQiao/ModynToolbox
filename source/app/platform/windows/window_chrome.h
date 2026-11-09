#pragma once
#include <QtGlobal>
class QWidget;
class QToolButton;
bool configureWindowFrame(QWidget *window);
bool handleWindowFrame(QWidget *window, QWidget *titleBar, QToolButton *maximize, void *message, qintptr *result);
