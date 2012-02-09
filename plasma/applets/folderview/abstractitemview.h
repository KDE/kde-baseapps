/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#ifndef ABSTRACTITEMVIEW_H
#define ABSTRACTITEMVIEW_H

#include <QGraphicsWidget>
#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QPointer>
#include <QCache>
#include <QTextLayout>
#include <QTime>
#include <QBasicTimer>

#include <plasma/widgets/scrollbar.h>
#include <plasma/framesvg.h>


class KDirModel;
class KFileItemDelegate;
class QItemSelectionModel;
class ProxyModel;
class QScrollBar;


// The abstract base class for IconView and ListView
class AbstractItemView : public QGraphicsWidget
{
    Q_OBJECT

    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
    Q_PROPERTY(bool drawShadows READ drawShadows WRITE setDrawShadows)

public:
    enum ScrollDirection { ScrollUp, ScrollDown };

    AbstractItemView(QGraphicsWidget *parent = 0);
    ~AbstractItemView();

    virtual void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void setSelectionModel(QItemSelectionModel *model);
    QItemSelectionModel *selectionModel() const;

    void setItemDelegate(KFileItemDelegate *delegate);
    KFileItemDelegate *itemDelegate() const;

    virtual void setIconSize(const QSize &iconSize);
    QSize iconSize() const;

    void setDrawShadows(bool on);
    bool drawShadows() const;

    QScrollBar *verticalScrollBar() const;

    QRect visibleArea() const;
    virtual QModelIndex indexAt(const QPointF &point) const = 0;
    virtual QRect visualRect(const QModelIndex &index) const = 0;

    QPointF mapToViewport(const QPointF &point) const;
    QRectF mapToViewport(const QRectF &rect) const;
    QPointF mapFromViewport(const QPointF &point) const;
    QRectF mapFromViewport(const QRectF &rect) const;

    void markAreaDirty(const QRect &rect);
    void markAreaDirty(const QRectF &rect) { markAreaDirty(rect.toAlignedRect()); }

    void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint =  QAbstractItemView::EnsureVisible);
    void autoScroll(ScrollDirection direction, int pixelsPerSecond);
    void stopAutoScrolling();

signals:
    void activated(const QModelIndex &index);
    void entered(const QModelIndex &index);
    void left(const QModelIndex &index);
    void contextMenuRequest(QWidget *widget, const QPoint &screenPos);

protected:
    QRect scrollBackBuffer();
    void prepareBackBuffer();
    void syncBackBuffer(QPainter *painter, const QRect &clipRect);

    QSize doTextLayout(QTextLayout &layout, const QSize &constraints, Qt::Alignment alignment,
                       QTextOption::WrapMode wrapMode) const;
    void drawTextLayout(QPainter *painter, const QTextLayout &layout, const QRect &rect) const; 

    void timerEvent(QTimerEvent *event);

    void smoothScroll(int dx, int dy);
    void startScrolling();
    void stopScrolling();
    void scrollTick();

protected slots:
    virtual void rowsInserted(const QModelIndex &parent, int first, int last);
    virtual void rowsRemoved(const QModelIndex &parent, int first, int last);
    virtual void modelReset();
    virtual void layoutChanged();
    virtual void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    virtual void commitData(QWidget *editor);
    virtual void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);

    virtual void finishedScrolling();
    virtual void svgChanged();

private slots:
    void scrollBarValueChanged(int value);
    void scrollBarActionTriggered(int action);
    void scrollBarSliderReleased();

protected:
    Plasma::FrameSvg *m_itemFrame;
    KFileItemDelegate *m_delegate;
    QPointer<KDirModel> m_dirModel;
    QPointer<ProxyModel> m_model;
    QPointer<QItemSelectionModel> m_selectionModel;
    QSize m_iconSize;
    QRegion m_dirtyRegion;
    QPixmap m_pixmap;
    QPixmap m_topFadeTile;
    QPixmap m_bottomFadeTile;
    Plasma::ScrollBar *m_scrollBar;
    QStyle *m_style;
    QWidget *m_styleWidget;
    int m_lastScrollValue;
    bool m_viewScrolled;

    // These variables are for the smooth scrolling code
    int m_dx;
    int m_ddx;
    int m_dddx;
    int m_rdx;
    int m_dy;
    int m_ddy;
    int m_dddy;
    int m_rdy;
    bool m_smoothScrolling;
    QBasicTimer m_smoothScrollTimer;
    QBasicTimer m_autoScrollTimer;
    QTime m_smoothScrollStopwatch;
    QTime m_autoScrollTime;
    ScrollDirection m_scrollDirection;
    int m_autoScrollSpeed;
    int m_autoScrollSetSpeed;
    bool m_drawShadows;
};

inline QPointF AbstractItemView::mapToViewport(const QPointF &point) const
{
    return point + QPointF(0, m_scrollBar->value());
}

inline QRectF AbstractItemView::mapToViewport(const QRectF &rect) const
{
    return rect.translated(0, m_scrollBar->value());
}

inline QPointF AbstractItemView::mapFromViewport(const QPointF &point) const
{
    return point - QPointF(0, m_scrollBar->value());
}

inline QRectF AbstractItemView::mapFromViewport(const QRectF &rect) const
{
    return rect.translated(0, -m_scrollBar->value());
}

#endif // ABSTRACTITEMVIEW_H

