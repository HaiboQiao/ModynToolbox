#pragma once
#include <QGraphicsView>
class QGraphicsVideoItem;
class QVideoSink;
// A widget-scene video surface can move between floating and scrollable tool views
// without creating/reparenting a native child window container.
class VideoSurface : public QGraphicsView {
    Q_OBJECT
  public:
    explicit VideoSurface(QWidget *parent = nullptr);
    QVideoSink *videoSink() const;
    QObject *videoOutput() const;

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    QGraphicsVideoItem *item;
};
