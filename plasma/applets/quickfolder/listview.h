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

#ifndef LISTVIEW_H
#define LISTVIEW_H

#include "abstractitemview.h"

#include <QCache>


class QStyleOptionViewItemV4;
class Animator;

namespace Plasma {
    class FrameSvg;
}

class ListView : public AbstractItemView
{
    Q_OBJECT

public:
    ListView(QGraphicsWidget *parent = 0);
    ~ListView();

    void setModel(QAbstractItemModel *model);

    void setIconSize(const QSize &iconSize);

    void setWordWrap(bool on);
    bool wordWrap() const;

    void setTextLineCount(int rows);
    int textLineCount() const;

    QModelIndex indexAt(const QPointF &point) const;
    QRect visualRect(const QModelIndex &index) const;

protected:
    void startDrag(const QPointF &pos, QWidget *widget);
    void updateScrollBar();
    void updateSizeHint();
    QStyleOptionViewItemV4 viewOptions() const;

    QSize itemSize(const QStyleOptionViewItemV4 &option, const QModelIndex &index) const;
    void paintItem(QPainter *painter, const QStyleOptionViewItemV4 &option, const QModelIndex &index) const;
    
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);
    void resizeEvent(QGraphicsSceneResizeEvent *event);

    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void modelReset();
    void layoutChanged();
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private slots:
    void svgChanged();

private:
    Animator *m_animator;
    int m_rowHeight;
    int m_numTextLines;
    QPersistentModelIndex m_pressedIndex;
    bool m_dragInProgress;
    bool m_wordWrap;
    bool m_drawShadows;
};

#endif // LISTVIEW_H
