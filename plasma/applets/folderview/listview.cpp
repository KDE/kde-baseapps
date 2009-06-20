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

#include "listview.h"

#include <QApplication>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QItemSelectionModel>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include <KDirModel>
#include <KFileItemDelegate>
#include <KGlobalSettings>

#include "proxymodel.h"

#include "plasma/containment.h"
#include "plasma/corona.h"
#include "plasma/paintutils.h"
#include "plasma/theme.h"


ListView::ListView(QGraphicsWidget *parent)
    : AbstractItemView(parent),
      m_rowHeight(-1),
      m_dragInProgress(false),
      m_wordWrap(true),
      m_drawShadows(true)
{
    setAcceptHoverEvents(true);
    setAcceptDrops(true);
    setCacheMode(NoCache);
}

ListView::~ListView()
{
}

void ListView::setModel(QAbstractItemModel *model)
{
    AbstractItemView::setModel(model);
    updateSizeHint();
}

void ListView::setIconSize(const QSize &size)
{
    if (size != m_iconSize) {
        m_iconSize = size;
        m_rowHeight = -1;
        markAreaDirty(visibleArea());
    }
}

void ListView::setWordWrap(bool on)
{
    if (m_wordWrap != on) {
        m_wordWrap = on;
        m_rowHeight = -1;
        markAreaDirty(visibleArea());
    }
}

bool ListView::wordWrap() const
{
    return m_wordWrap;
}

void ListView::setDrawShadows(bool on)
{
    if (m_drawShadows != on) {
        m_drawShadows = on;
        markAreaDirty(visibleArea());
    }
}

bool ListView::drawShadows() const
{
    return m_drawShadows;
}

void ListView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    markAreaDirty(visibleArea());
    updateScrollBar();
    updateSizeHint();
}

void ListView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    markAreaDirty(visibleArea());
    updateScrollBar();
    updateSizeHint();
}

void ListView::modelReset()
{
    markAreaDirty(visibleArea());
    updateScrollBar();
    updateSizeHint();
}

void ListView::layoutChanged()
{
    markAreaDirty(visibleArea());
    updateScrollBar();
    updateSizeHint();
}

void ListView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    markAreaDirty(visualRect(topLeft) | visualRect(bottomRight));
}

void ListView::updateScrollBar()
{
    if (!m_model) {
        return;
    }

    if (m_rowHeight == -1 && m_model->rowCount() > 0) {
        // Use the height of the first item for all items
        const QSize size = m_delegate->sizeHint(viewOptions(), m_model->index(0, 0));
        m_rowHeight = size.height();
    }

    int max = int(m_rowHeight * m_model->rowCount() - contentsRect().height());

    // Keep the scrollbar handle at the bottom if it was at the bottom and the viewport
    // has grown vertically
    bool updateValue = (m_scrollBar->minimum() != m_scrollBar->maximum()) &&
            (max > m_scrollBar->maximum()) && (m_scrollBar->value() == m_scrollBar->maximum());

    m_scrollBar->setRange(0, max);
    m_scrollBar->setPageStep(contentsRect().height());
    m_scrollBar->setSingleStep(m_rowHeight);

    if (updateValue) {
        m_scrollBar->setValue(max);
    }

    if (max > 0) {
        m_scrollBar->show();
    } else {
        m_scrollBar->hide();
    }
}

void ListView::updateSizeHint()
{
    if (m_rowHeight == -1 && m_model->rowCount() > 0) {
        // Use the height of the first item for all items
        const QSize size = m_delegate->sizeHint(viewOptions(), m_model->index(0, 0));
        m_rowHeight = size.height();
    }

    QFontMetrics fm(font());
    setPreferredSize(m_iconSize.width() + fm.lineSpacing() * 18, m_rowHeight * m_model->rowCount());
}

QRect ListView::visualRect(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_model->rowCount()) {
        return QRect();
    }

    QRectF cr = contentsRect();
    return QRect(cr.left(), cr.top() + index.row() * m_rowHeight, cr.width(), m_rowHeight);
}

void ListView::updateTextShadows(const QColor &textColor)
{
    if (!m_drawShadows) {
        m_delegate->setShadowColor(Qt::transparent);
        return;
    }

    QColor shadowColor;

    // Use black shadows with bright text, and white shadows with dark text.
    if (qGray(textColor.rgb()) > 192) {
        shadowColor = Qt::black;
    } else {
        shadowColor = Qt::white;
    }

    if (m_delegate->shadowColor() != shadowColor)
    {
        m_delegate->setShadowColor(shadowColor);

        // Center white shadows to create a halo effect, and offset dark shadows slightly.
        if (shadowColor == Qt::white) {
            m_delegate->setShadowOffset(QPoint(0, 0));
        } else {
            m_delegate->setShadowOffset(QPoint(layoutDirection() == Qt::RightToLeft ? -1 : 1, 1));
        }
    }
}

void ListView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    const QRect cr = contentsRect().toRect();
    if (!cr.isValid()) {
        return;
    }


    QRect clipRect = cr & option->exposedRect.toAlignedRect();
    if (clipRect.isEmpty()) {
        return;
    }
    int offset = m_scrollBar->value();

    prepareBackBuffer();

    painter->setClipRect(clipRect);


    // Update the dirty region in the backbuffer
    // =========================================
    if (!m_dirtyRegion.isEmpty()) {
        QStyleOptionViewItemV4 opt = viewOptions();
        int width = m_scrollBar->isVisible() ? cr.width() - m_scrollBar->geometry().width() : cr.width();

        if (m_rowHeight == -1 && m_model->rowCount() > 0) {
            // Use the height of the first item for all items
            const QSize size = m_delegate->sizeHint(opt, m_model->index(0, 0));
            m_rowHeight = size.height();
        }

        QPainter p(&m_pixmap);
        p.translate(-cr.topLeft() - QPoint(0, offset));
        p.setClipRegion(m_dirtyRegion);

        // Clear the dirty region
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(mapToViewport(cr).toAlignedRect(), Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        for (int i = 0; i < m_model->rowCount(); i++) {
            opt.rect = QRect(cr.left(), cr.top() + i * m_rowHeight, width, m_rowHeight);

            if (!m_dirtyRegion.intersects(opt.rect)) {
                continue;
            }

            const QModelIndex index = m_model->index(i, 0);
            opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Selected);

            if (m_selectionModel->isSelected(index)) {
                if (m_dragInProgress) {
                    continue;
                }
                updateTextShadows(palette().color(QPalette::HighlightedText));
                opt.state |= QStyle::State_Selected | QStyle::State_MouseOver;
            } else {
                updateTextShadows(palette().color(QPalette::Text));
            }

            if (hasFocus() && index == m_selectionModel->currentIndex()) {
                opt.state |= QStyle::State_HasFocus;
            }

            m_delegate->paint(&p, opt, index);
        }

        m_dirtyRegion = QRegion();
    }

    syncBackBuffer(painter, clipRect);
}

QModelIndex ListView::indexAt(const QPointF &pos) const
{
    int row = pos.y() / m_rowHeight;
    return row < m_model->rowCount() ? m_model->index(row, 0) : QModelIndex();
}

void ListView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const QPoint pos = mapToViewport(event->pos()).toPoint();
    const QModelIndex index = indexAt(pos);

    if (m_selectionModel->currentIndex().isValid()) {
        markAreaDirty(visualRect(m_selectionModel->currentIndex()));
    }

    if (index.isValid()) {
        m_selectionModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        markAreaDirty(visualRect(index));
    }
}

void ListView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QPoint pos = mapToViewport(event->pos()).toPoint();
    const QModelIndex index = indexAt(pos);

    if (index != m_selectionModel->currentIndex()) {
        markAreaDirty(visualRect(index));
        markAreaDirty(visualRect(m_selectionModel->currentIndex()));
        m_selectionModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    }
}

void ListView::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    if (!m_pressedIndex.isValid() && m_selectionModel->currentIndex().isValid()) {
        markAreaDirty(visualRect(m_selectionModel->currentIndex()));
        m_selectionModel->clear();
    }
}


void ListView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    const QPointF pos = mapToViewport(event->pos());
    //setFocus(Qt::MouseFocusReason);

    if (event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(pos);
        if (index.isValid()) {
            if (!m_selectionModel->isSelected(index)) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visibleArea());
            }
            event->ignore(); // Causes contextMenuEvent() to get called
        } else if (m_selectionModel->hasSelection()) {
            m_selectionModel->clearSelection();
            markAreaDirty(visibleArea());
        }
        m_pressedIndex = index;
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(pos);

        // If an icon was pressed
        if (index.isValid())
        {
            if (event->modifiers() & Qt::ControlModifier) {
                m_selectionModel->select(index, QItemSelectionModel::Toggle);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visualRect(index));
            } else if (!m_selectionModel->isSelected(index)) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visibleArea());
            }
            m_pressedIndex = index;
            return;
        }
    }
}

void ListView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QPointF pos = mapToViewport(event->pos());
        const QModelIndex index = indexAt(pos);

        if (index.isValid() && index == m_pressedIndex && !(event->modifiers() & Qt::ControlModifier)) {
            emit activated(index);
            m_selectionModel->clearSelection();
            markAreaDirty(visibleArea());
        }
    }

    m_pressedIndex = QModelIndex();
}

void ListView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }

    // If an item is pressed
    if (m_pressedIndex.isValid())
    {
        const QPointF buttonDownPos = event->buttonDownPos(Qt::LeftButton);
        const QPointF point = event->pos() - buttonDownPos;
        if (point.toPoint().manhattanLength() >= QApplication::startDragDistance())
        {
            startDrag(mapToViewport(buttonDownPos), event->widget());
        }
        return;
    }
}

void ListView::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if ((event->modifiers() & Qt::CTRL) || (event->orientation() == Qt::Horizontal)) {
        // Let the event propagate to the parent widget
        event->ignore();
        return;
    }

    const int pixels = 96 * event->delta() / 120;
    smoothScroll(0, -pixels);
}

void ListView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    const QPointF pos = mapToViewport(event->pos());
    const QModelIndex index = indexAt(pos);

    if (index.isValid()) {
        emit contextMenuRequest(event->widget(), event->screenPos());
    } else {
        // Let the event propagate to the parent widget
        event->ignore();
    }
}

void ListView::dropEvent(QGraphicsSceneDragDropEvent *)
{
    m_pressedIndex = QModelIndex();
}

void ListView::resizeEvent(QGraphicsSceneResizeEvent *)
{
    const QRectF cr = contentsRect();
    const QRectF r = QRectF(cr.right() - m_scrollBar->geometry().width(), cr.top(),
                            m_scrollBar->geometry().width(), cr.height());

    if (m_scrollBar->geometry() != r) {
        m_scrollBar->setGeometry(r);
    }

    updateScrollBar();
    markAreaDirty(visibleArea());
}

// pos is the position where the mouse was clicked in the applet.
// widget is the widget that sent the mouse event that triggered the drag.
void ListView::startDrag(const QPointF &pos, QWidget *widget)
{
    QModelIndexList indexes = m_selectionModel->selectedIndexes();
    QRect boundingRect;
    foreach (const QModelIndex &index, indexes) {
        boundingRect |= visualRect(index);
    }

    QPixmap pixmap(boundingRect.size());
    pixmap.fill(Qt::transparent);

    QStyleOptionViewItemV4 option = viewOptions();
    // ### We can't draw the items as selected or hovered since Qt doesn't
    //     use an ARGB window for the drag pixmap.
    //option.state |= QStyle::State_Selected | QStyle::State_MouseOver;
    option.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);

    updateTextShadows(palette().color(QPalette::HighlightedText));

    QPainter p(&pixmap);
    foreach (const QModelIndex &index, indexes)
    {
        option.rect = visualRect(index).translated(-boundingRect.topLeft());
        m_delegate->paint(&p, option, index);
    }
    p.end();

    // Mark the area containing the about-to-be-dragged items as dirty, so they
    // will be erased from the view on the next repaint.  We have to do this
    // before calling QDrag::exec(), since it's a blocking call.
    markAreaDirty(boundingRect);

    m_dragInProgress = true;

    QDrag *drag = new QDrag(widget);
    drag->setMimeData(m_model->mimeData(indexes));
    drag->setPixmap(pixmap);
    drag->setHotSpot((pos - boundingRect.topLeft()).toPoint());
    drag->exec(m_model->supportedDragActions());

    m_dragInProgress = false;

    // Repaint the dragged icons in case the drag did not remove the file
    markAreaDirty(boundingRect);
}

QStyleOptionViewItemV4 ListView::viewOptions() const
{
    QStyleOptionViewItemV4 option;
    initStyleOption(&option);

    option.font                = font();
    option.decorationAlignment = Qt::AlignCenter;
    option.decorationPosition  = QStyleOptionViewItem::Left;
    option.decorationSize      = iconSize();
    option.displayAlignment    = Qt::AlignLeft | Qt::AlignVCenter;
    option.textElideMode       = Qt::ElideMiddle;
    option.locale              = QLocale::system();
    option.widget              = m_styleWidget;
    option.viewItemPosition    = QStyleOptionViewItemV4::OnlyOne;

    if (m_wordWrap) {
        option.features = QStyleOptionViewItemV2::WrapText;
    }

    return option;
}

#include "listview.moc"
