#pragma once
#include "ui/view_spec.h"
class QWidget;
class QPlainTextEdit;
struct RenderedView {
    QWidget *widget = nullptr;
    QPlainTextEdit *editor = nullptr;
};
RenderedView renderToolView(const ViewSpec &spec);
