#include "tool_navigation.h"
#include "ui/theme/theme.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

ToolNavigation::ToolNavigation(QWidget *parent) : QTreeWidget(parent) {
    setObjectName("toolNavigation");
    setAccessibleName(QStringLiteral("工具和会话"));
    setColumnCount(2);
    setHeaderHidden(true);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::Fixed);
    setColumnWidth(1, Theme::navigationActionWidth);
    setIndentation(Theme::navigationIndent);
    setIconSize(QSize(18, 18));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setExpandsOnDoubleClick(false);
    setUniformRowHeights(true);
    setMouseTracking(true);
}
void ToolNavigation::drawBranches(QPainter *painter, const QRect &rect,
                                  const QModelIndex &index) const {
    painter->save();
    const int middle = rect.center().y();
    const int x = rect.right() - indentation() / 2;
    if (index.parent().isValid()) {
        painter->setPen(Theme::border);
        const bool last = index.row() + 1 == model()->rowCount(index.parent());
        painter->drawLine(x, rect.top(), x, last ? middle : rect.bottom());
        painter->drawLine(x, middle, rect.right(), middle);
    } else if (model()->hasChildren(index)) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(Theme::muted, 1.3));
        if (isExpanded(index)) {
            painter->drawLine(x - 3, middle - 2, x, middle + 1);
            painter->drawLine(x, middle + 1, x + 3, middle - 2);
        } else {
            painter->drawLine(x - 2, middle - 3, x + 1, middle);
            painter->drawLine(x + 1, middle, x - 2, middle + 3);
        }
    }
    painter->restore();
}
void ToolNavigation::cancelDrag() {
    pressed = QPersistentModelIndex();
    dragging = false;
    plusPressed = false;
    viewport()->unsetCursor();
    QToolTip::hideText();
}
void ToolNavigation::mousePressEvent(QMouseEvent *event) {
    cancelDrag();
    releaseCancelled = false;
    auto *item = itemAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && item) {
        start = event->position().toPoint();
        plusPressed = columnAt(start.x()) == 1 && !item->parent() && !item->text(1).isEmpty();
        if (plusPressed || (columnAt(start.x()) == 0 && start.x() >= visualItemRect(item).left()))
            pressed = indexFromItem(item);
        if (plusPressed) {
            event->accept();
            return;
        }
    }
    QTreeWidget::mousePressEvent(event);
}
void ToolNavigation::mouseMoveEvent(QMouseEvent *event) {
    if (pressed.isValid() && !plusPressed && event->buttons().testFlag(Qt::LeftButton)) {
        if (!dragging && (event->position().toPoint() - start).manhattanLength() >=
                             QApplication::startDragDistance())
            dragging = pressed.data(SessionRole).toInt() > 0;
        if (dragging) {
            viewport()->setCursor(Qt::ClosedHandCursor);
            const bool outside =
                !window()->frameGeometry().contains(event->globalPosition().toPoint());
            QToolTip::showText(event->globalPosition().toPoint() + QPoint(16, 18),
                               pressed.data(DragLabelRole).toString() +
                                   (outside ? QStringLiteral("\n松开以独立窗口打开")
                                            : QStringLiteral("\n拖出主窗口以独立打开 · Esc 取消")),
                               this);
            event->accept();
            return;
        }
    }
    QTreeWidget::mouseMoveEvent(event);
}
void ToolNavigation::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QTreeWidget::mouseReleaseEvent(event);
        return;
    }
    const auto index = pressed;
    const auto tool = index.data(ToolRole).toString();
    const int session = index.data(SessionRole).toInt();
    const auto position = event->globalPosition().toPoint();
    const bool detach = dragging && !window()->frameGeometry().contains(position);
    const bool create = plusPressed &&
                        itemAt(event->position().toPoint()) == itemFromIndex(index) &&
                        columnAt(event->position().toPoint().x()) == 1;
    const bool click = index.isValid() && !dragging && !plusPressed && !releaseCancelled;
    const bool wasPlus = plusPressed;
    cancelDrag();
    if (!wasPlus)
        QTreeWidget::mouseReleaseEvent(event);
    if (detach)
        emit detachRequested(tool, session, position);
    else if (create)
        emit createRequested(tool);
    else if (click)
        emit openRequested(tool, session);
}
void ToolNavigation::openCurrent() {
    if (auto *item = currentItem())
        emit openRequested(item->data(0, ToolRole).toString(), item->data(0, SessionRole).toInt());
}
void ToolNavigation::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && pressed.isValid()) {
        cancelDrag();
        releaseCancelled = true;
        event->accept();
        return;
    }
    auto *item = currentItem();
    if (item && event->key() == Qt::Key_N && event->modifiers() == Qt::ControlModifier) {
        emit createRequested(item->data(0, ToolRole).toString());
        event->accept();
        return;
    }
    QTreeWidget::keyPressEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Up || event->key() == Qt::Key_Down ||
        event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
        openCurrent();
}
void ToolNavigation::contextMenuEvent(QContextMenuEvent *event) {
    cancelDrag();
    auto *item =
        event->reason() == QContextMenuEvent::Keyboard ? currentItem() : itemAt(event->pos());
    if (!item)
        return;
    setCurrentItem(item);
    const auto point = event->reason() == QContextMenuEvent::Keyboard
                           ? viewport()->mapToGlobal(visualItemRect(item).center())
                           : event->globalPos();
    emit menuRequested(item->data(0, ToolRole).toString(), item->data(0, SessionRole).toInt(),
                       point);
}
