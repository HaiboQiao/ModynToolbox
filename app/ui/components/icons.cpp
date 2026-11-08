#include "icons.h"
#include <QPainter>
#include <QFont>
#include <QLinearGradient>
QIcon uiIcon(UiIcon icon, const QColor &color) {
    QPixmap image(40, 40);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (icon) {
    case UiIcon::Search: p.drawEllipse(QRectF(3,3,10,10)); p.drawLine(QPointF(12,12),QPointF(17,17)); break;
    case UiIcon::Add: p.drawLine(4,10,16,10); p.drawLine(10,4,10,16); break;
    case UiIcon::External: p.drawLine(10,3,17,3); p.drawLine(17,3,17,10); p.drawLine(17,3,8,12); p.drawLine(4,4,4,16); p.drawLine(4,16,16,16); break;
    case UiIcon::Return: p.drawLine(5,5,5,12); p.drawLine(5,12,12,12); p.drawLine(5,12,15,3); p.drawLine(3,16,17,16); break;
    case UiIcon::Folder: { QPolygonF path{{2,5},{8,5},{10,7},{18,7},{18,16},{2,16},{2,5}}; p.drawPolyline(path); break; }
    case UiIcon::Image: p.drawRoundedRect(QRectF(2,3,16,14),2,2); p.drawEllipse(QRectF(5,6,3,3)); p.drawLine(3,15,9,10); p.drawLine(9,10,13,14); p.drawLine(13,14,16,11); break;
    case UiIcon::Code: p.drawPolyline(QPolygonF{{6,4},{2,10},{6,16}}); p.drawPolyline(QPolygonF{{14,4},{18,10},{14,16}}); p.drawLine(12,3,8,17); break;
    case UiIcon::Files: p.drawRoundedRect(QRectF(5,2,12,15),1,1); p.drawLine(2,5,2,19); p.drawLine(2,19,13,19); p.drawLine(8,7,14,7); p.drawLine(8,11,14,11); break;
    case UiIcon::Minimize: p.drawLine(5,11,15,11); break;
    case UiIcon::Maximize: p.drawRect(QRectF(5,5,10,10)); break;
    case UiIcon::Restore: p.drawRect(QRectF(4,7,9,9)); p.drawPolyline(QPolygonF{{7,7},{7,4},{16,4},{16,13},{13,13}}); break;
    case UiIcon::Close: p.drawLine(5,5,15,15); p.drawLine(15,5,5,15); break;
    }
    return QIcon(image);
}
QIcon brandIcon() {
    QPixmap image(64,64);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0,0,64,64);
    gradient.setColorAt(0,QColor("#ac9af0"));
    gradient.setColorAt(1,QColor("#7054d8"));
    p.setBrush(gradient);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(1,1,62,62),16,16);
    QFont font("Segoe UI",27,QFont::DemiBold);
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(image.rect(),Qt::AlignCenter,QStringLiteral("M"));
    return QIcon(image);
}
