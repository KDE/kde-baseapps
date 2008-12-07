/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Rafael Fernández López <ereslibre@kde.org>
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

#ifndef ICONVIEW_H
#define ICONVIEW_H

#include "abstractitemview.h"

#include <QAbstractItemDelegate>
#include <QListView>
#include <QPointer>
#include <QCache>
#include <QTime>
#include <QBasicTimer>


class KUrl;
class KDirModel;
class KFileItemDelegate;
class KFileItemList;
class KFilePreviewGenerator;
class KNewMenu;
class QItemSelectionModel;
class ProxyModel;
class QStyleOptionViewItemV4;
class QScrollBar;

namespace Plasma
{
    class ScrollBar;
}

struct ViewItem
{
    ViewItem() : rect(QRect()), layouted(false) {}
    QRect rect;
    bool layouted;
};

class IconView : public AbstractItemView
{
    Q_OBJECT

    Q_PROPERTY(QSize gridSize READ gridSize WRITE setGridSize)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool alignToGrid READ alignToGrid WRITE setAlignToGrid)
    Q_PROPERTY(bool iconsMoveable READ iconsMoveable WRITE setIconsMoveable)
    Q_PROPERTY(bool drawShadows READ drawShadows WRITE setDrawShadows)
    Q_PROPERTY(QListView::Flow flow READ flow WRITE setFlow)

public:
    IconView(QGraphicsWidget *parent);
    ~IconView();

    void setModel(QAbstractItemModel *model);

    void setGridSize(const QSize &gridSize);
    QSize gridSize() const;

    void setIconSize(const QSize &gridSize);

    void setWordWrap(bool on);
    bool wordWrap() const;

    void setFlow(QListView::Flow flow);
    QListView::Flow flow() const;

    void setAlignToGrid(bool on);
    bool alignToGrid() const;

    void setIconsMoveable(bool on);
    bool iconsMoveable() const;

    void setDrawShadows(bool on);
    bool drawShadows() const;

    void setIconPositionsData(const QStringList &data);
    QStringList iconPositionsData() const;

    void renameSelectedIcon();

    QRect visualRect(const QModelIndex &index) const;
    QRegion visualRegion(const QModelIndex &index) const;
    QModelIndex indexAt(const QPointF &point) const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

signals:
    void indexesMoved(const QModelIndexList &indexes);
    void busy(bool);

protected:
    bool indexIntersectsRect(const QModelIndex &index, const QRect &rect) const;
    void startDrag(const QPointF &pos, QWidget *widget);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);
    void timerEvent(QTimerEvent *event);
    void changeEvent(QEvent *event);
    void resizeEvent(QGraphicsSceneResizeEvent *event);

    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void modelReset();
    void layoutChanged();
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void commitData(QWidget *editor);
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);

private slots:
    void listingStarted(const KUrl &url);
    void listingClear();
    void listingCompleted();
    void listingCanceled();
    void listingError(const QString &message);
    void itemsDeleted(const KFileItemList &items);

private:
    void paintErrorMessage(QPainter *painter, const QRect &rect, const QString &message) const;
    int columnsForWidth(qreal width) const;
    int rowsForHeight(qreal height) const;
    QPoint nextGridPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const;
    QPoint findNextEmptyPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const;
    void layoutItems();
    void alignIconsToGrid();
    bool doLayoutSanityCheck();
    void saveIconPositions() const;
    void loadIconPositions();
    void updateScrollBar();
    void updateScrollBarGeometry();
    void updateTextShadows(const QColor &textColor);
    QStyleOptionViewItemV4 viewOptions() const;

private:
    QVector<ViewItem> m_items;
    QHash<QString, QPoint> m_savedPositions;
    mutable QCache<quint64, QRegion> m_regionCache;
    int m_columns;
    int m_rows;
    int m_validRows;
    bool m_layoutBroken;
    bool m_needPostLayoutPass;
    bool m_initialListing;
    bool m_positionsLoaded;
    bool m_doubleClick;
    bool m_dragInProgress;
    bool m_iconsLocked;
    bool m_alignToGrid;
    bool m_wordWrap;
    bool m_drawShadows;
    QPersistentModelIndex m_hoveredIndex;
    QPersistentModelIndex m_pressedIndex;
    QPersistentModelIndex m_editorIndex;
    QRect m_rubberBand;
    QRectF m_viewportRect;
    QPointF m_buttonDownPos;
    QTime m_pressTime;
    QListView::Flow m_flow;
    QString m_errorMessage;
    QPoint m_lastDeletedPos;
    QPoint m_currentLayoutPos;
    QSize m_gridSize;
    QBasicTimer m_delayedLayoutTimer;
    QBasicTimer m_delayedCacheClearTimer;
};

#endif

