#include "combo_box.h"
#include "ui/theme/theme.h"
#include <QPainter>
void UiComboBox::paintEvent(QPaintEvent *event) {
    QComboBox::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(isEnabled() ? Theme::muted : Theme::border,1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    const QPointF center(width()-16,height()/2.0);
    painter.drawLine(center+QPointF(-3,-1.5),center+QPointF(0,1.5));
    painter.drawLine(center+QPointF(0,1.5),center+QPointF(3,-1.5));
}
