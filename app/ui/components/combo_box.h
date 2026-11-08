#pragma once
#include <QComboBox>
class UiComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent *event) override;
};
