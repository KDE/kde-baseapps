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

#include "actionoverlay.h"

#include <Plasma/PaintUtils>
#include <Plasma/Animator>
#include <Plasma/Svg>

#include <QPainter>
#include <QGraphicsGridLayout>

ActionIcon::ActionIcon(QGraphicsItem* parent)
    : QGraphicsWidget(parent),
      m_pressed(false),
      m_sunken(false)
{
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);

    m_icon = new Plasma::Svg(this);
    m_icon->setImagePath("widgets/action-overlays");
    m_icon->setContainsMultipleImages(true);

    setMinimumSize(m_icon->elementSize("add-normal"));
    setMaximumSize(minimumSize());

    show();
}

void ActionIcon::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget()->parentWidget());
    QPersistentModelIndex index = static_cast<ActionOverlay*>(parentWidget())->hoverIndex();
    QItemSelectionModel *m_selectionModel = view->selectionModel();

    QString element = m_selectionModel->isSelected(index) ? "remove" : "add";
    if (m_sunken) {
        element += "-pressed";
    } else if (isUnderMouse()) {
        element += "-hover";
    } else {
        element += "-normal";
    }
    m_icon->paint(painter, rect(), element);
}

void ActionIcon::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_pressed = true;
    m_sunken = true;
    update();
}

void ActionIcon::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_pressed = false;
    m_sunken = false;
    if (isUnderMouse()) {
        emit clicked();
    }
    update();
}

void ActionIcon::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_sunken != isUnderMouse()) {
        m_sunken = isUnderMouse();
        update();
    }
}

void ActionIcon::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    emit iconHoverEnter();
    update();
}

void ActionIcon::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    emit iconHoverLeave();
    update();
}


// --------------------------------------------------------


ActionOverlay::ActionOverlay(AbstractItemView* parent)
        : QGraphicsWidget(parent)
{
    QGraphicsGridLayout *lay = new QGraphicsGridLayout(this);

    m_iconToggleSelection = new ActionIcon(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addItem(m_iconToggleSelection, 0, 0);

    connect(parentWidget(), SIGNAL(entered(QModelIndex)), this, SLOT(entered(QModelIndex)));
    connect(parentWidget(), SIGNAL(left(QModelIndex)), this, SLOT(left(QModelIndex)));
    connect(parentWidget(), SIGNAL(modelChanged()), this, SLOT(modelChanged()));

    connect(m_iconToggleSelection, SIGNAL(clicked()), this, SLOT(selected()));

    m_hideActionOverlayIconTimer = new QTimer(this);
    connect(m_hideActionOverlayIconTimer, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(m_iconToggleSelection, SIGNAL(iconHoverEnter()), m_hideActionOverlayIconTimer, SLOT(stop()));
    connect(m_iconToggleSelection, SIGNAL(iconHoverLeave()), m_hideActionOverlayIconTimer, SLOT(start()));

    connect(parent->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(close()));

    m_hideActionOverlayIconTimer->setInterval(500);
    m_hideActionOverlayIconTimer->setSingleShot(true);

    fadeIn = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    fadeIn->setProperty("startOpacity", 0);
    fadeIn->setProperty("targetOpacity", 1);
    fadeIn->setWidgetToAnimate(this);

    fadeOut = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    fadeOut->setProperty("startOpacity", 1);
    fadeOut->setProperty("targetOpacity", 0);
    fadeOut->setWidgetToAnimate(this);

    hide();
}

void ActionOverlay::selected()
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());
    QItemSelectionModel *m_selectionModel = view->selectionModel();

    if (m_hoverIndex.isValid()) {
        m_selectionModel->select(m_hoverIndex, QItemSelectionModel::Toggle);
        m_selectionModel->setCurrentIndex(m_hoverIndex, QItemSelectionModel::NoUpdate);
        view->markAreaDirty(view->visibleArea());
    }
}

QPersistentModelIndex ActionOverlay::hoverIndex()
{
    return m_hoverIndex;
}

void ActionOverlay::entered(const QModelIndex &index)
{
    m_hideActionOverlayIconTimer->stop();

    if (index.isValid()) {
        AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());
        setPos(view->mapFromViewport(view->visualRect(index)).topLeft());
        show();
        if (m_hoverIndex != index) {
            m_iconToggleSelection->update();
            fadeIn->start();
        }
        m_hoverIndex = index;
    }
}

void ActionOverlay::left(const QModelIndex &index)
{
    Q_UNUSED(index);

    if (!m_hideActionOverlayIconTimer->isActive()) {
        m_hideActionOverlayIconTimer->start();
    }
}

void ActionOverlay::timeout()
{
    // allow the animation to restart after hiding the ActionOverlayIcon even if m_hoverIndex didn't change
    m_hoverIndex = QPersistentModelIndex();

    if (isVisible() && (fadeOut->state() != QAbstractAnimation::Running)) {
        fadeOut->start();
        connect(fadeOut, SIGNAL(finished()), SLOT(close()));
    }
}

void ActionOverlay::forceHide(HideHint hint)
{
    m_hideActionOverlayIconTimer->stop();
    if (hint == FadeOut) {
        timeout();
    } else {
        hide();
    }
}

void ActionOverlay::rowsRemoved(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    if (!m_hoverIndex.isValid()) {
        hide();
    }
}

void ActionOverlay::modelChanged()
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());

    QAbstractItemModel *mod = view->model();
    connect(mod, SIGNAL(rowsRemoved(QModelIndex, int, int)), SLOT(rowsRemoved(QModelIndex, int, int)));
}

