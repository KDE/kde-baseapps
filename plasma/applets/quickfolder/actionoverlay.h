/*
 *   Copyright © 2009, 2010 Fredrik Höglund <fredrik@kde.org>
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

#include <Plasma/Animation>

#include <QTimer>
#include <QGraphicsWidget>
#include <QPersistentModelIndex>
#include <QGraphicsGridLayout>

namespace Plasma {
    class Svg;
}

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
    void setElement(const QString &element);

signals:
    void clicked();
    void iconHoverEnter();
    void iconHoverLeave();

private:
    Plasma::Svg *m_icon;
    QString m_element;
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

    void setShowFolderButton(bool show);
    void setShowSelectionButton(bool show);
    bool showFolderButton() const;
    bool showSelectionButton() const;

    QSizeF iconSize() const;

private slots:
    void toggleSelection();
    void openPopup();
    void entered(const QModelIndex &index);
    void left(const QModelIndex &index);
    void timeout();
    void modelChanged();
    void rowsRemoved(const QModelIndex& indexes, int start, int end);
    void checkIfFolderResult(const QModelIndex &index, bool isFolder);
    void toggleShowActionButton(bool show, ActionIcon *button, unsigned int pos);

private:
    ActionIcon *m_toggleButton;
    ActionIcon *m_openButton;
    QPersistentModelIndex m_hoverIndex;
    QTimer *m_hideActionOverlayIconTimer;
    Plasma::Animation *fadeIn;
    Plasma::Animation *fadeOut;
    bool m_showFolderButton;
    bool m_showSelectionButton;
    QGraphicsGridLayout * m_layout;
};

#endif // ACTIONOVERLAY_H
