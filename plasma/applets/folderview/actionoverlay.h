/*
 *   Copyright © 2009 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2009 Bruno Bigras <bigras.bruno@gmail.com>
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

#ifndef ACTIONOVERLAY_H
#define ACTIONOVERLAY_H

#include "abstractitemview.h"

#include <Plasma/AbstractAnimation>

#include <QTimer>
#include <QGraphicsWidget>
#include <QPersistentModelIndex>

class ActionIcon : public QGraphicsWidget
{
    Q_OBJECT

public:
    ActionIcon(QGraphicsItem* parent = 0);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

signals:
    void clicked();
    void iconHoverEnter();
    void iconHoverLeave();

private:
    bool m_pressed;
    bool m_sunken;
};


// -----------------------------------------------------------------------


class ActionOverlay : public QGraphicsWidget
{
    Q_OBJECT

public:
    enum HideHint { HideNow, FadeOut };

    ActionOverlay(AbstractItemView *parent = 0);
    QPersistentModelIndex hoverIndex();
    void forceHide(HideHint hint);

private slots:
    void selected();
    void entered(const QModelIndex &index);
    void left(const QModelIndex &index);
    void timeout();
    void modelChanged();
    void rowsRemoved(const QModelIndex& indexes, int start, int end);

private:
    ActionIcon *m_iconToggleSelection;
    QPersistentModelIndex m_hoverIndex;
    QTimer *m_hideActionOverlayIconTimer;
    Plasma::AbstractAnimation *fadeIn;
    Plasma::AbstractAnimation *fadeOut;
};

#endif // ACTIONOVERLAY_H
