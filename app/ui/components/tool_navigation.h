#pragma once
#include <QPersistentModelIndex>
#include <QTreeWidget>

class ToolNavigation : public QTreeWidget {
    Q_OBJECT
  public:
    enum Role { ToolRole = Qt::UserRole, SearchRole, CategoryRole, SessionRole, DragLabelRole };
    explicit ToolNavigation(QWidget *parent = nullptr);

  signals:
    void openRequested(const QString &tool, int session);
    void createRequested(const QString &tool);
    void detachRequested(const QString &tool, int session, const QPoint &position);
    void menuRequested(const QString &tool, int session, const QPoint &position);

  protected:
    void drawBranches(QPainter *painter, const QRect &rect,
                      const QModelIndex &index) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

  private:
    void cancelDrag();
    void openCurrent();
    QPersistentModelIndex pressed;
    QPoint start;
    bool dragging = false, plusPressed = false, releaseCancelled = false;
};
