/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
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

#include "iconwidget.h"

#include <KDirModel>
#include <KDirLister>

#include <konq_operations.h>

#include <Plasma/Corona>
#include <QGraphicsSceneDragDropEvent>


IconWidget::IconWidget(QGraphicsItem *parent)
    : Plasma::IconWidget(parent), m_model(0)
{
    setAcceptDrops(true);
}

IconWidget::~IconWidget()
{
}

void IconWidget::setModel(KDirModel *model)
{
    m_model = model;
}

void IconWidget::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (KUrl::List::canDecode(event->mimeData())){
        Plasma::IconWidget::sceneEventFilter(this, event);
        event->accept();
    }
}

void IconWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Plasma::IconWidget::sceneEventFilter(this, event);
}

void IconWidget::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    event->setAccepted(!event->mimeData()->hasFormat(appletMimeType));
}

void IconWidget::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    // If the dropped item is an applet, let the parent widget handle it
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    if (event->mimeData()->hasFormat(appletMimeType)) {
        event->ignore();
        return;
    }

    QDropEvent ev(event->screenPos(), event->dropAction(), event->mimeData(),
                  event->buttons(), event->modifiers());
    KonqOperations::doDrop(m_model->dirLister()->rootItem(), m_model->dirLister()->url(),
                           &ev, event->widget());
}

