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

#include "folderview.h"
#include "folderview.moc"

#include <QApplication>
#include <QDebug>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <KDirLister>
#include <KDirModel>
#include <KFileItemDelegate>


FolderView::FolderView(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args)
{
    setHasConfigurationInterface(false);
    setAcceptHoverEvents(true);
    //setDrawStandardBackground(false);
    //setContentSize(600, 400);

    m_model = new KDirModel(this);
    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(modelReset()), SLOT(modelReset()));
    connect(m_model, SIGNAL(layoutChanged()), SLOT(layoutChanged()));
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(layoutChanged(QModelIndex,QModelIndex)));

    KDirLister *lister = new KDirLister(this);
    lister->openUrl(KUrl(QDir::homePath()));

    m_model->setDirLister(lister);

    m_delegate = new KFileItemDelegate(this);
    m_selectionModel = new QItemSelectionModel(m_model, this);
    m_layoutValid = false;
}

FolderView::~FolderView()
{
}

QSizeF FolderView::contentSizeHint() const
{
    return QSizeF(600, 400);
}

void FolderView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    m_layoutValid = false;
    update();
}

void FolderView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    m_layoutValid = false;
    update();
}

void FolderView::modelReset()
{
    m_layoutValid = false;
    update();
}

void FolderView::layoutChanged()
{
    m_layoutValid = false;
    update();
}

void FolderView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)

    m_layoutValid = false;
    update();
}

int FolderView::columnsForWidth(qreal width) const
{
    int spacing = 10;
    int margin = 10;

    qreal available = width - 2 * margin + spacing;
    return qFloor(available / (92 + spacing));
}

void FolderView::layoutItems() const
{
    QStyleOptionViewItemV4 option = styleOption();
    m_items.resize(m_model->rowCount());

    int spacing = 10;
    int margin = 10;
    int x = margin; 
    int y = margin;

    QSize gridSize(92, 92);
    int rowHeight = 0;
    int maxColumns = columnsForWidth(contentRect().width());
    int column = 0;

    for (int i = 0; i < m_items.size(); i++) {
        const QModelIndex index = m_model->index(i, 0);
        QSize size = m_delegate->sizeHint(option, index).boundedTo(gridSize);

        QPoint pos(x + (gridSize.width() - size.width()) / 2, y);
        m_items[i].rect = QRect(pos, size);

        rowHeight = qMax(rowHeight, size.height());
        x += gridSize.width() + spacing;

        if (++column >= maxColumns) {
            y += rowHeight + spacing;
            rowHeight = 0;
            column = 0;
            x = margin;
        }
    }

    m_columns = maxColumns;
    m_layoutValid = true;
}

void FolderView::paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentRect)
{
    QStyleOptionViewItemV4 opt = styleOption();
    opt.palette.setColor(QPalette::All, QPalette::Text, Qt::white);

    if (!m_layoutValid || columnsForWidth(contentRect.width()) != m_columns)
        layoutItems();

    const QRect clipRect = contentRect & option->exposedRect.toAlignedRect();
    if (clipRect.isEmpty())
        return;

    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    for (int i = 0; i < m_items.size(); i++) {
        opt.rect = m_items[i].rect;

        if (!opt.rect.intersects(clipRect))
            continue;

        const QModelIndex index = m_model->index(i, 0);
        opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Selected);

        if (index == m_hoveredIndex)
            opt.state |= QStyle::State_MouseOver;

        if (m_selectionModel->isSelected(index))
            opt.state |= QStyle::State_Selected;

        if (hasFocus() && index == m_selectionModel->currentIndex())
            opt.state |= QStyle::State_HasFocus;

        m_delegate->paint(painter, opt, index);
    }

    painter->restore();
}

QModelIndex FolderView::indexAt(const QPointF &point) const
{
    if (!m_layoutValid)
        layoutItems();

    if (!contentRect().contains(point))
        return QModelIndex();

    for (int i = 0; i < m_items.size(); i++) {
        if (m_items[i].rect.contains(point.toPoint()))
            return m_model->index(i, 0);
    }

    return QModelIndex();
}

QRectF FolderView::visualRect(const QModelIndex &index) const
{
    if (!m_layoutValid)
        layoutItems();

    if (!index.isValid() || index.row() < 0 || index.row() > m_items.size())
        return QRectF();

    return m_items[index.row()].rect;
}

void FolderView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        m_hoveredIndex = index;
        update(visualRect(index));
    }
}

void FolderView::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_hoveredIndex.isValid()) {
        update(visualRect(m_hoveredIndex));
        m_hoveredIndex = QModelIndex();
    }
}

void FolderView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index != m_hoveredIndex) {
        update(visualRect(index));
        update(visualRect(m_hoveredIndex));
        m_hoveredIndex = index;
    }
}

void FolderView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            if (event->modifiers() & Qt::ControlModifier) {
                m_selectionModel->select(index, QItemSelectionModel::Toggle);
                update(visualRect(index));
            } else {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                update();
            }
            m_pressedIndex = index;
        } else {
            if (m_selectionModel->hasSelection()) {
                m_selectionModel->clearSelection();
                update();
            }
        }
    }

    Plasma::Applet::mousePressEvent(event);
}

void FolderView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            if (index == m_pressedIndex) {
                // This is where we activate the item (if it isn't being dragged)
            }
        }
    }

    m_pressedIndex = QModelIndex();

    Plasma::Applet::mouseReleaseEvent(event);
}

Qt::Orientations FolderView::expandingDirections() const
{
    return Qt::Vertical | Qt::Horizontal;
}

void FolderView::showConfigurationInterface()
{
}

QStyleOptionViewItemV4 FolderView::styleOption() const
{
    QStyleOptionViewItemV4 option;
    option.direction           = QApplication::layoutDirection();
    option.font                = QApplication::font();
    option.fontMetrics         = QFontMetrics(option.font);
    option.palette             = QApplication::palette();
    option.rect                = QRect();
    option.state               = QStyle::State_Enabled | QStyle::State_Active;
    option.decorationAlignment = Qt::AlignTop | Qt::AlignHCenter;
    option.decorationPosition  = QStyleOptionViewItem::Top;
    option.decorationSize      = QSize(48, 48);
    option.displayAlignment    = Qt::AlignHCenter;
    option.textElideMode       = Qt::ElideRight;
    //option.features            = QStyleOptionViewItemV2::WrapText;
    option.locale              = QLocale::system();
    option.widget              = 0;
    option.viewItemPosition    = QStyleOptionViewItemV4::OnlyOne;

    return option;
}

