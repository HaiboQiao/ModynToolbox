#pragma once
#include <QSpinBox>

// Preserve native stepping, keyboard input and accessibility with explicit chevrons.
class UiSpinBox : public QSpinBox {
  public:
    using QSpinBox::QSpinBox;
  protected:
    void paintEvent(QPaintEvent *event) override;
};
