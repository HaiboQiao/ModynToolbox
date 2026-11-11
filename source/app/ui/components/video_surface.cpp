#include "video_surface.h"
#include "ui/theme/theme.h"
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QResizeEvent>
VideoSurface::VideoSurface(QWidget *parent) : QGraphicsView(parent) {
    auto *content = new QGraphicsScene(this);
    setScene(content);
    item = new QGraphicsVideoItem;
    item->setAspectRatioMode(Qt::KeepAspectRatio);
    content->addItem(item);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignCenter);
    setBackgroundBrush(Theme::videoBackground);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
}
QVideoSink *VideoSurface::videoSink() const { return item->videoSink(); }
QObject *VideoSurface::videoOutput() const { return item; }
void VideoSurface::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    const QSizeF size = viewport()->size();
    item->setSize(size);
    setSceneRect(QRectF(QPointF(), size));
}
