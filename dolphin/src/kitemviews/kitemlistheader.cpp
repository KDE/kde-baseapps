/***************************************************************************
 *   Copyright (C) 2011 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "kitemlistheader_p.h"

#include <KAction>
#include <KMenu>
#include "kitemmodelbase.h"

#include <QApplication>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QStyleOptionHeader>

#include <KDebug>

KItemListHeader::KItemListHeader(QGraphicsWidget* parent) :
    QGraphicsWidget(parent),
    m_model(0),
    m_visibleRoles(),
    m_visibleRolesWidths(),
    m_hoveredRoleIndex(-1),
    m_pressedRoleIndex(-1),
    m_roleOperation(NoRoleOperation),
    m_pressedMousePos(),
    m_movingRole()
{
    m_movingRole.x = 0;
    m_movingRole.xDec = 0;
    m_movingRole.index = -1;

    setAcceptHoverEvents(true);

    QStyleOptionHeader option;
    const QSize headerSize = style()->sizeFromContents(QStyle::CT_HeaderSection, &option, QSize());
    resize(0, headerSize.height());
}

KItemListHeader::~KItemListHeader()
{
}

void KItemListHeader::setModel(KItemModelBase* model)
{
    if (m_model == model) {
        return;
    }

    if (m_model) {
        disconnect(m_model, SIGNAL(sortRoleChanged(QByteArray,QByteArray)),
                   this, SLOT(slotSortRoleChanged(QByteArray,QByteArray)));
        disconnect(m_model, SIGNAL(sortOrderChanged(Qt::SortOrder,Qt::SortOrder)),
                   this, SLOT(slotSortOrderChanged(Qt::SortOrder,Qt::SortOrder)));
    }

    m_model = model;

    if (m_model) {
        connect(m_model, SIGNAL(sortRoleChanged(QByteArray,QByteArray)),
                this, SLOT(slotSortRoleChanged(QByteArray,QByteArray)));
        connect(m_model, SIGNAL(sortOrderChanged(Qt::SortOrder,Qt::SortOrder)),
                this, SLOT(slotSortOrderChanged(Qt::SortOrder,Qt::SortOrder)));
    }
}

KItemModelBase* KItemListHeader::model() const
{
    return m_model;
}

void KItemListHeader::setVisibleRoles(const QList<QByteArray>& roles)
{
    m_visibleRoles = roles;
    update();
}

QList<QByteArray> KItemListHeader::visibleRoles() const
{
    return m_visibleRoles;
}

void KItemListHeader::setVisibleRolesWidths(const QHash<QByteArray, qreal> rolesWidths)
{
    m_visibleRolesWidths = rolesWidths;

    // Assure that no width is smaller than the minimum allowed width
    const qreal minWidth = minimumRoleWidth();
    QMutableHashIterator<QByteArray, qreal> it(m_visibleRolesWidths);
    while (it.hasNext()) {
        it.next();
        if (it.value() < minWidth) {
            m_visibleRolesWidths.insert(it.key(), minWidth);
        }
    }

    update();
}

QHash<QByteArray, qreal> KItemListHeader::visibleRolesWidths() const
{
    return m_visibleRolesWidths;
}

qreal KItemListHeader::minimumRoleWidth() const
{
    QFontMetricsF fontMetrics(font());
    return fontMetrics.height() * 4;
}

void KItemListHeader::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (!m_model) {
        return;
    }

    // Draw roles
    painter->setFont(font());
    painter->setPen(palette().text().color());

    qreal x = 0;
    int orderIndex = 0;
    foreach (const QByteArray& role, m_visibleRoles) {
        const qreal roleWidth = m_visibleRolesWidths.value(role);
        const QRectF rect(x, 0, roleWidth, size().height());
        paintRole(painter, role, rect, orderIndex);
        x += roleWidth;
        ++orderIndex;
    }

    // Draw background without roles
    QStyleOption opt;
    opt.init(widget);
    opt.rect = QRect(x, 0, size().width() - x, size().height());
    opt.state |= QStyle::State_Horizontal;
    style()->drawControl(QStyle::CE_HeaderEmptyArea, &opt, painter);

    if (!m_movingRole.pixmap.isNull()) {
        Q_ASSERT(m_roleOperation == MoveRoleOperation);
        painter->drawPixmap(m_movingRole.x, 0, m_movingRole.pixmap);
    }
}

void KItemListHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() & Qt::LeftButton) {
        updatePressedRoleIndex(event->pos());
        m_pressedMousePos = event->pos();
        m_roleOperation = isAboveRoleGrip(m_pressedMousePos, m_pressedRoleIndex) ?
                          ResizeRoleOperation : NoRoleOperation;
        event->accept();
    } else {
        event->ignore();
    }
}

void KItemListHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsWidget::mouseReleaseEvent(event);

    if (m_pressedRoleIndex == -1) {
        return;
    }

    switch (m_roleOperation) {
    case NoRoleOperation: {
        // Only a click has been done and no moving or resizing has been started
        const QByteArray sortRole = m_model->sortRole();
        const int sortRoleIndex = m_visibleRoles.indexOf(sortRole);
        if (m_pressedRoleIndex == sortRoleIndex) {
            // Toggle the sort order
            const Qt::SortOrder previous = m_model->sortOrder();
            const Qt::SortOrder current = (m_model->sortOrder() == Qt::AscendingOrder) ?
                                          Qt::DescendingOrder : Qt::AscendingOrder;
            m_model->setSortOrder(current);
            emit sortOrderChanged(current, previous);
        } else {
            // Change the sort role
            const QByteArray previous = m_model->sortRole();
            const QByteArray current = m_visibleRoles[m_pressedRoleIndex];
            m_model->setSortRole(current);
            emit sortRoleChanged(current, previous);
        }
        break;
    }

    case MoveRoleOperation:
        m_movingRole.pixmap = QPixmap();
        m_movingRole.x = 0;
        m_movingRole.xDec = 0;
        m_movingRole.index = -1;
        break;

    default:
        break;
    }

    m_pressedRoleIndex = -1;
    m_roleOperation = NoRoleOperation;
    update();

    QApplication::restoreOverrideCursor();
}

void KItemListHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsWidget::mouseMoveEvent(event);

    switch (m_roleOperation) {
    case NoRoleOperation:
        if ((event->pos() - m_pressedMousePos).manhattanLength() >= QApplication::startDragDistance()) {
            // A role gets dragged by the user. Create a pixmap of the role that will get
            // synchronized on each furter mouse-move-event with the mouse-position.
            m_roleOperation = MoveRoleOperation;
            const int roleIndex = roleIndexAt(m_pressedMousePos);
            m_movingRole.index = roleIndex;
            if (roleIndex == 0) {
                // TODO: It should be configurable whether moving the first role is allowed.
                // In the context of Dolphin this is not required, however this should be
                // changed if KItemViews are used in a more generic way.
                QApplication::setOverrideCursor(QCursor(Qt::ForbiddenCursor));
            } else {
                m_movingRole.pixmap = createRolePixmap(roleIndex);

                qreal roleX = 0;
                for (int i = 0; i < roleIndex; ++i) {
                    const QByteArray role = m_visibleRoles[i];
                    roleX += m_visibleRolesWidths.value(role);
                }

                m_movingRole.xDec = event->pos().x() - roleX;
                m_movingRole.x = roleX;
                update();
            }
        }
        break;

    case ResizeRoleOperation: {
        const QByteArray pressedRole = m_visibleRoles[m_pressedRoleIndex];

        qreal previousWidth = m_visibleRolesWidths.value(pressedRole);
        qreal currentWidth = previousWidth;
        currentWidth += event->pos().x() - event->lastPos().x();
        currentWidth = qMax(minimumRoleWidth(), currentWidth);

        m_visibleRolesWidths.insert(pressedRole, currentWidth);
        update();

        emit visibleRoleWidthChanged(pressedRole, currentWidth, previousWidth);
        break;
    }

    case MoveRoleOperation: {
        // TODO: It should be configurable whether moving the first role is allowed.
        // In the context of Dolphin this is not required, however this should be
        // changed if KItemViews are used in a more generic way.
        if (m_movingRole.index > 0) {
            m_movingRole.x = event->pos().x() - m_movingRole.xDec;
            update();

            const int targetIndex = targetOfMovingRole();
            if (targetIndex > 0 && targetIndex != m_movingRole.index) {
                const QByteArray role = m_visibleRoles[m_movingRole.index];
                const int previousIndex = m_movingRole.index;
                m_movingRole.index = targetIndex;
                emit visibleRoleMoved(role, targetIndex, previousIndex);

                m_movingRole.xDec = event->pos().x() - roleXPosition(role);
            }
        }
        break;
    }

    default:
        break;
    }
}

void KItemListHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsWidget::hoverEnterEvent(event);
    updateHoveredRoleIndex(event->pos());
}

void KItemListHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsWidget::hoverLeaveEvent(event);
    if (m_hoveredRoleIndex != -1) {
        m_hoveredRoleIndex = -1;
        update();
    }
}

void KItemListHeader::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsWidget::hoverMoveEvent(event);

    const QPointF& pos = event->pos();
    updateHoveredRoleIndex(pos);
    if (m_hoveredRoleIndex >= 0 && isAboveRoleGrip(pos, m_hoveredRoleIndex)) {
        setCursor(Qt::SplitHCursor);
    } else {
        unsetCursor();
    }
}

void KItemListHeader::slotSortRoleChanged(const QByteArray& current, const QByteArray& previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    update();
}

void KItemListHeader::slotSortOrderChanged(Qt::SortOrder current, Qt::SortOrder previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    update();
}

void KItemListHeader::paintRole(QPainter* painter,
                                const QByteArray& role,
                                const QRectF& rect,
                                int orderIndex) const
{
    // The following code is based on the code from QHeaderView::paintSection().
    // Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
    QStyleOptionHeader option;
    option.section = orderIndex;
    option.state = QStyle::State_None | QStyle::State_Raised | QStyle::State_Horizontal;
    if (isEnabled()) {
        option.state |= QStyle::State_Enabled;
    }
    if (window() && window()->isActiveWindow()) {
        option.state |= QStyle::State_Active;
    }
    if (m_hoveredRoleIndex == orderIndex) {
        option.state |= QStyle::State_MouseOver;
    }
    if (m_pressedRoleIndex == orderIndex) {
        option.state |= QStyle::State_Sunken;
    }
    if (m_model->sortRole() == role) {
        option.sortIndicator = (m_model->sortOrder() == Qt::AscendingOrder) ?
                                QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp;
    }
    option.rect = rect.toRect();

    if (m_visibleRoles.count() == 1) {
        option.position = QStyleOptionHeader::OnlyOneSection;
    } else if (orderIndex == 0) {
        option.position = QStyleOptionHeader::Beginning;
    } else if (orderIndex == m_visibleRoles.count() - 1) {
        option.position = QStyleOptionHeader::End;
    } else {
        option.position = QStyleOptionHeader::Middle;
    }

    option.orientation = Qt::Horizontal;
    option.selectedPosition = QStyleOptionHeader::NotAdjacent;
    option.text = m_model->roleDescription(role);

    style()->drawControl(QStyle::CE_Header, &option, painter);
}

void KItemListHeader::updatePressedRoleIndex(const QPointF& pos)
{
    const int pressedIndex = roleIndexAt(pos);
    if (m_pressedRoleIndex != pressedIndex) {
        m_pressedRoleIndex = pressedIndex;
        update();
    }
}

void KItemListHeader::updateHoveredRoleIndex(const QPointF& pos)
{
    const int hoverIndex = roleIndexAt(pos);
    if (m_hoveredRoleIndex != hoverIndex) {
        m_hoveredRoleIndex = hoverIndex;
        update();
    }
}

int KItemListHeader::roleIndexAt(const QPointF& pos) const
{
    int index = -1;

    qreal x = 0;
    foreach (const QByteArray& role, m_visibleRoles) {
        ++index;
        x += m_visibleRolesWidths.value(role);
        if (pos.x() <= x) {
            break;
        }
    }

    return index;
}

bool KItemListHeader::isAboveRoleGrip(const QPointF& pos, int roleIndex) const
{
    qreal x = 0;
    for (int i = 0; i <= roleIndex; ++i) {
        const QByteArray role = m_visibleRoles[i];
        x += m_visibleRolesWidths.value(role);
    }

    const int grip = style()->pixelMetric(QStyle::PM_HeaderGripMargin);
    return pos.x() >= (x - grip) && pos.x() <= x;
}

QPixmap KItemListHeader::createRolePixmap(int roleIndex) const
{
    const QByteArray role = m_visibleRoles[roleIndex];
    const qreal roleWidth = m_visibleRolesWidths.value(role);
    const QRect rect(0, 0, roleWidth, size().height());

    QImage image(rect.size(), QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&image);
    paintRole(&painter, role, rect, roleIndex);

    // Apply a highlighting-color
    const QPalette::ColorGroup group = isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    QColor highlightColor = palette().color(group, QPalette::Highlight);
    highlightColor.setAlpha(64);
    painter.fillRect(rect, highlightColor);

    // Make the image transparent
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(0, 0, image.width(), image.height(), QColor(0, 0, 0, 192));

    return QPixmap::fromImage(image);
}

int KItemListHeader::targetOfMovingRole() const
{
    const int movingWidth = m_movingRole.pixmap.width();
    const int movingLeft = m_movingRole.x;
    const int movingRight = movingLeft + movingWidth - 1;

    int targetIndex = 0;
    qreal targetLeft = 0;
    while (targetIndex < m_visibleRoles.count()) {
        const QByteArray role = m_visibleRoles[targetIndex];
        const qreal targetWidth = m_visibleRolesWidths.value(role);
        const qreal targetRight = targetLeft + targetWidth - 1;

        const bool isInTarget = (targetWidth >= movingWidth &&
                                 movingLeft  >= targetLeft  &&
                                 movingRight <= targetRight) ||
                                (targetWidth <  movingWidth &&
                                 movingLeft  <= targetLeft  &&
                                 movingRight >= targetRight);

        if (isInTarget) {
            return targetIndex;
        }

        targetLeft += targetWidth;
        ++targetIndex;
    }

    return m_movingRole.index;
}

qreal KItemListHeader::roleXPosition(const QByteArray& role) const
{
    qreal x = 0;
    foreach (const QByteArray& visibleRole, m_visibleRoles) {
        if (visibleRole == role) {
            return x;
        }

        x += m_visibleRolesWidths.value(visibleRole);
    }

    return -1;
}

#include "kitemlistheader_p.moc"
