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

#include <QPainter>
#include <QGraphicsGridLayout>

ActionIcon::ActionIcon(QGraphicsItem* parent)
    : QGraphicsWidget(parent), m_pressed(false)
{
    setMinimumSize(24, 24);
    setMaximumSize(24, 24);
    setAcceptHoverEvents(true);
    show();
}

void ActionIcon::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget()->parentWidget());
    QPersistentModelIndex index = static_cast<ActionOverlay*>(parentWidget())->hoverIndex();
    QItemSelectionModel *m_selectionModel = view->selectionModel();

    QLinearGradient gradient(.25, 0, .75, 1);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    if (isUnderMouse()) {
        gradient.setColorAt(0, QColor(160, 160, 160));
        gradient.setColorAt(1, QColor(96, 96, 96));
    } else {
        gradient.setColorAt(0, QColor(128, 128, 128));
        gradient.setColorAt(1, QColor(64, 64, 64));
    }

    QPainterPath plus;
    plus.setFillRule(Qt::WindingFill);

    // if the item is selected we only draw a minus sign instead of a plus
    if (!m_selectionModel->isSelected(index)) {
        plus.addRect(24 / 2 - 1, 8, 2, 8);
    }
    plus.addRect(8, 24 / 2 - 1, 8, 2);

    QImage image(24, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(0);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(image.rect().adjusted(3, 3, -3, -3));
    p.setBrush(gradient);
    p.drawEllipse(QRectF(image.rect()).adjusted(4.5, 4.5, -4.5, -4.5));
    p.setBrush(Qt::white);
    p.drawPath(plus);
    p.end();

    QImage shadow = image;
    Plasma::PaintUtils::shadowBlur(shadow, 1, QColor(0, 0, 0, 192));

    QRectF r = geometry();

    painter->drawImage(r.topLeft() - QPoint(2, 2), shadow);
    painter->drawImage(r.topLeft() - (m_pressed ? QPoint(2, 2) : QPoint(3, 3)), image);

    QGraphicsWidget::paint(painter, option, widget);
}

void ActionIcon::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_pressed = true;
    update();
}

void ActionIcon::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_pressed = false;
    if (isUnderMouse()) {
        emit clicked();
    }
    update();
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

    connect(m_iconToggleSelection, SIGNAL(clicked()), this, SLOT(selected()));

    m_hideActionOverlayIconTimer = new QTimer(this);
    connect(m_hideActionOverlayIconTimer, SIGNAL(timeout()), this, SLOT(close()));
    connect(m_iconToggleSelection, SIGNAL(iconHoverEnter()), m_hideActionOverlayIconTimer, SLOT(stop()));
    connect(m_iconToggleSelection, SIGNAL(iconHoverLeave()), m_hideActionOverlayIconTimer, SLOT(start()));

    connect(parent->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(close()));

    m_hideActionOverlayIconTimer->setInterval(500);

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
