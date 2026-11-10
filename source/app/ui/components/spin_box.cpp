#include "spin_box.h"
#include "ui/theme/theme.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSpinBox>

void UiSpinBox::paintEvent(QPaintEvent *event) {
    QSpinBox::paintEvent(event);
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto control : {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown}) {
        const auto rect = style()->subControlRect(QStyle::CC_SpinBox, &option, control, this);
        if (rect.isEmpty() || buttonSymbols() == QAbstractSpinBox::NoButtons)
            continue;
        const bool up = control == QStyle::SC_SpinBoxUp;
        const bool enabled = isEnabled() && option.stepEnabled.testFlag(
            up ? QAbstractSpinBox::StepUpEnabled : QAbstractSpinBox::StepDownEnabled);
        painter.setPen(QPen(enabled ? Theme::muted : Theme::border, 1.6,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF center = QRectF(rect).center();
        const qreal direction = up ? -1 : 1;
        painter.drawPolyline(QPolygonF{center + QPointF(-3, -1.5 * direction),
                                      center + QPointF(0, 1.5 * direction),
                                      center + QPointF(3, -1.5 * direction)});
    }
}
