/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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
#include "popupview.h"

#include <QAbstractItemDelegate>
#include <QPointer>
#include <QCache>
#include <QTime>
#include <QBasicTimer>


class KUrl;
class KDirModel;
class KFileItemDelegate;
class KFileItemList;
class KFilePreviewGenerator;
class KJob;
class KNewMenu;
class KonqOperations;
class QItemSelectionModel;
class ProxyModel;
class QActionGroup;
class QStyleOptionViewItemV4;
class QScrollBar;
class ToolTipWidget;

namespace Plasma
{
    class ScrollBar;
}

struct ViewItem
{
    ViewItem() : rect(QRect()), layouted(false), needSizeAdjust(true) {}
    QRect rect;
    bool layouted:1;
    bool needSizeAdjust:1;
};

class IconView : public AbstractItemView
{
    Q_OBJECT

public:
    enum Flow { LeftToRight, TopToBottom, RightToLeft, TopToBottomRightToLeft };
    enum ToolTipType { FolderTip, FileTip };

    Q_PROPERTY(QSize gridSize READ gridSize WRITE setGridSize)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool alignToGrid READ alignToGrid WRITE setAlignToGrid)
    Q_PROPERTY(bool iconsMoveable READ iconsMoveable WRITE setIconsMoveable)
    Q_PROPERTY(bool drawShadows READ drawShadows WRITE setDrawShadows)
    Q_PROPERTY(bool customLayout READ customLayout WRITE setCustomLayout)
    Q_PROPERTY(Flow flow READ flow WRITE setFlow)

public:
    IconView(QGraphicsWidget *parent);
    ~IconView();

    void setModel(QAbstractItemModel *model);

    void setGridSize(const QSize &gridSize);
    QSize gridSize() const;

    void setIconSize(const QSize &gridSize);

    void setWordWrap(bool on);
    bool wordWrap() const;

    void setFlow(Flow flow);
    Flow flow() const;

    void setAlignToGrid(bool on);
    bool alignToGrid() const;

    void setIconsMoveable(bool on);
    bool iconsMoveable() const;

    void setDrawShadows(bool on);
    bool drawShadows() const;

    void setCustomLayout(bool value);
    bool customLayout() const;

    void setIconPositionsData(const QStringList &data);
    QStringList iconPositionsData() const;

    bool renameInProgress() const;
    bool dragInProgress() const;
    bool popupVisible() const;

    int scrollBarExtent() const;
    QSize sizeForRowsColumns(int rows, int columns) const;

    QRect visualRect(const QModelIndex &index) const;
    QRegion visualRegion(const QModelIndex &index) const;
    QModelIndex indexAt(const QPointF &point) const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    void triggerToolTip(ToolTipType type);

signals:
    void indexesMoved(const QModelIndexList &indexes);
    void popupViewClosed();
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
    void keyPressEvent(QKeyEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
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

    void finishedScrolling();

public slots:
    void renameSelectedIcon();

private slots:
    void listingStarted(const KUrl &url);
    void listingClear();
    void listingCompleted();
    void listingCanceled();
    void listingError(const QString &message);
    void itemsDeleted(const KFileItemList &items);
    void popupCloseRequested();
    void dropActionTriggered(QAction *action);
    void dropCompleted();
    void statResult(KJob *job);
    void repositionWidgetsManually();

private:
    void paintErrorMessage(QPainter *painter, const QRect &rect, const QString &message) const;
    int columnsForWidth(qreal width) const;
    int rowsForHeight(qreal height) const;
    QPoint nextGridPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const;
    QPoint findNextEmptyPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const;
    void layoutItems();
    void alignIconsToGrid();
    QRect itemsBoundingRect() const;
    bool doLayoutSanityCheck();
    void saveIconPositions() const;
    void loadIconPositions();
    void updateScrollBar();
    void updateScrollBarGeometry();
    void updateTextShadows(const QColor &textColor);
    void updateToolTip(QWidget *causedWidget = 0);
    void createDropActions(const KUrl::List &urls, QActionGroup *actions);
    QStyleOptionViewItemV4 viewOptions() const;

private:
    QVector<ViewItem> m_items;
    QHash<QString, QPoint> m_savedPositions;
    mutable QCache<quint64, QRegion> m_regionCache;
    qreal m_margins[4];
    int m_columns;
    int m_rows;
    int m_validRows;
    bool m_layoutBroken;
    bool m_needPostLayoutPass;
    bool m_initialListing;
    bool m_positionsLoaded;
    bool m_doubleClick;
    bool m_dragInProgress;
    bool m_hoverDrag;
    bool m_iconsLocked;
    bool m_alignToGrid;
    bool m_wordWrap;
    bool m_drawShadows;
    QPersistentModelIndex m_hoveredIndex;
    QPersistentModelIndex m_pressedIndex;
    QPersistentModelIndex m_editorIndex;
    QRect m_rubberBand;
    QPointF m_buttonDownPos;
    QTime m_pressTime;
    Flow m_flow;
    QString m_errorMessage;
    QPoint m_lastDeletedPos;
    QPoint m_currentLayoutPos;
    QSize m_gridSize;
    QBasicTimer m_delayedLayoutTimer;
    QBasicTimer m_delayedRelayoutTimer;
    QBasicTimer m_delayedCacheClearTimer;
    QBasicTimer m_toolTipShowTimer;
    ToolTipWidget *m_toolTipWidget;
    QPointer<PopupView> m_popupView;
    QPointer<QWidget> m_popupCausedWidget;
    KUrl m_popupUrl;
    KonqOperations *m_dropOperation;
    QActionGroup *m_dropActions;
    QPersistentModelIndex m_popupIndex;
    QPointer<QGraphicsProxyWidget> m_editorProxy;
};

#endif

