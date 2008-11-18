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
#include "plasma/widgets/scrollbar.h"

#ifdef Q_WS_X11
#  include <QX11Info>
#  include <X11/Xlib.h>

#  undef FontChange
#endif

ListView::ListView(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
      m_lastScrollValue(0),
      m_rowHeight(-1),
      m_viewScrolled(false),
      m_dragInProgress(false),
      m_wordWrap(true),
      m_drawShadows(true)
{
    setAcceptHoverEvents(true);
    setAcceptDrops(true);
    setCacheMode(NoCache);

    m_scrollBar = new Plasma::ScrollBar(Qt::Vertical, this);
    connect(m_scrollBar->nativeWidget(), SIGNAL(valueChanged(int)), SLOT(scrollBarValueChanged(int)));

    int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_iconSize = QSize(size, size);
}

ListView::~ListView()
{
}

void ListView::setModel(QAbstractItemModel *model)
{
    m_model = static_cast<ProxyModel*>(model);

    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(modelReset()), SLOT(modelReset()));
    connect(m_model, SIGNAL(layoutChanged()), SLOT(layoutChanged()));
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(dataChanged(QModelIndex,QModelIndex)));

    updateSizeHint();
}

QAbstractItemModel *ListView::model() const
{
    return m_model;
}

void ListView::setSelectionModel(QItemSelectionModel *model)
{
    m_selectionModel = model;
}

QItemSelectionModel *ListView::selectionModel() const
{
    return m_selectionModel;
}

void ListView::setItemDelegate(KFileItemDelegate *delegate)
{
    m_delegate = static_cast<KFileItemDelegate*>(delegate);
}

KFileItemDelegate *ListView::itemDelegate() const
{
    return m_delegate;
}

void ListView::setIconSize(const QSize &size)
{
    if (size != m_iconSize) {
        m_iconSize = size;
        m_rowHeight = -1;
        markAreaDirty(visibleArea());
    }
}

QSize ListView::iconSize() const
{
    return m_iconSize;
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

QRect ListView::visibleArea() const
{
    return mapToViewport(contentsRect()).toAlignedRect();
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

void ListView::scrollBarValueChanged(int value)
{
    Q_UNUSED(value)

    m_viewScrolled = true;
    update();
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
    m_scrollBar->setSingleStep(10);

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
    setPreferredSize(256, m_rowHeight * m_model->rowCount());
}

// Marks the given rect in viewport coordinates, as dirty and schedules a repaint.
void ListView::markAreaDirty(const QRect &rect)
{
    if (!rect.isEmpty() && rect.intersects(visibleArea())) {
        m_dirtyRegion += rect;
        update(mapFromViewport(rect));
    }
}

QPointF ListView::mapToViewport(const QPointF &point) const
{
    return point + QPointF(0, m_scrollBar->value());
}

QRectF ListView::mapToViewport(const QRectF &rect) const
{
    return rect.translated(0, m_scrollBar->value());
}

QPointF ListView::mapFromViewport(const QPointF &point) const
{
    return point - QPointF(0, m_scrollBar->value());
}

QRectF ListView::mapFromViewport(const QRectF &rect) const
{
    return rect.translated(0, -m_scrollBar->value());
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

// This function scrolls the contents of the backbuffer the distance the scrollbar
// has moved since the last time this function was called.
QRect ListView::scrollBackbufferContents()
{
    int value =  m_scrollBar->value();
    int delta = m_lastScrollValue - value;
    m_lastScrollValue = value;

    if (qAbs(delta) >= m_pixmap.height()) {
        return mapToViewport(contentsRect()).toAlignedRect();
    }

    int sy, dy, h;
    QRect dirty;
    if (delta < 0) {
        dy = 0;
        sy = -delta;
        h = m_pixmap.height() - sy;
        dirty = QRect(0, m_pixmap.height() - sy, m_pixmap.width(), sy);
    } else {
        dy = delta;
        sy = 0;
        h = m_pixmap.height() - dy;
        dirty = QRect(0, 0, m_pixmap.width(), dy);
    }
#ifdef Q_WS_X11
    // Avoid the overhead of creating a QPainter to do the blit.
    Display *dpy = QX11Info::display();
    GC gc = XCreateGC(dpy, m_pixmap.handle(), 0, 0);
    XCopyArea(dpy, m_pixmap.handle(), m_pixmap.handle(), gc, 0, sy, m_pixmap.width(), h, 0, dy);
    XFreeGC(dpy, gc);
#else
    m_pixmap = m_pixmap.copy(0, sy, m_pixmap.width(), h);
#endif
    return mapToViewport(dirty.translated(contentsRect().topLeft().toPoint())).toAlignedRect();
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

    // Make sure the backbuffer pixmap has the same size as the content rect
    if (m_pixmap.isNull() || m_pixmap.size() != cr.size()) {
        m_pixmap = QPixmap(cr.size());
        m_pixmap.fill(Qt::transparent);
        m_dirtyRegion = QRegion(visibleArea());
    }

    if (m_viewScrolled) {
        m_dirtyRegion += scrollBackbufferContents();
        m_viewScrolled = false;
    }

    int offset = m_scrollBar->value();

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

            if (index == m_hoveredIndex) {
                opt.state |= QStyle::State_MouseOver;
            }

            if (m_selectionModel->isSelected(index)) {
                if (m_dragInProgress) {
                    continue;
                }
                updateTextShadows(palette().color(QPalette::HighlightedText));
                opt.state |= QStyle::State_Selected;
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

    const int fadeHeight = 16;
    const QRect topFadeRect(cr.x(), cr.y(), cr.width(), fadeHeight);
    const QRect bottomFadeRect(cr.bottomLeft() - QPoint(0, fadeHeight), QSize(cr.width(), fadeHeight));
    const int viewportHeight = m_model->rowCount() * m_rowHeight;

    // Draw the backbuffer on the Applet
    // =================================
    if ((offset > 0 && topFadeRect.intersects(clipRect)) ||
        (viewportHeight > (offset + cr.height()) && bottomFadeRect.intersects(clipRect)))
    {
        QPixmap pixmap = m_pixmap;
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        // Fade out the top section of the pixmap if the scrollbar slider isn't at the top
        if (offset > 0 && topFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_topFadeTile = QPixmap(256, fadeHeight);
                m_topFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, fadeHeight);
                g.setColorAt(0, Qt::transparent);
                g.setColorAt(1, Qt::black);
                QPainter p(&m_topFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, fadeHeight, g);
                p.end();
            }
            p.drawTiledPixmap(0, 0, m_pixmap.width(), fadeHeight, m_topFadeTile);
        }

        // Fade out the bottom part of the pixmap if the scrollbar slider isn't at the bottom
        if (viewportHeight > (offset + cr.height()) && bottomFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_bottomFadeTile = QPixmap(256, fadeHeight);
                m_bottomFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, fadeHeight);
                g.setColorAt(0, Qt::black);
                g.setColorAt(1, Qt::transparent);
                QPainter p(&m_bottomFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, fadeHeight, g);
                p.end();
            }
            p.drawTiledPixmap(0, m_pixmap.height() - fadeHeight, m_pixmap.width(), fadeHeight, m_bottomFadeTile);
        }
        p.end();

        painter->drawPixmap(cr.topLeft(), pixmap);
    }
    else
    {
        painter->drawPixmap(cr.topLeft(), m_pixmap);
    }
}

QModelIndex ListView::indexAt(const QPointF &pos) const
{
    int row = pos.y() / m_rowHeight;
    return row < m_model->rowCount() ? m_model->index(row, 0) : QModelIndex();
}

void ListView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QPoint pos = mapToViewport(event->pos()).toPoint();

    m_hoveredIndex = indexAt(pos);
    markAreaDirty(visualRect(m_hoveredIndex));
}

void ListView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPoint pos = mapToViewport(event->pos()).toPoint();

    QModelIndex index = indexAt(pos);
    if (index != m_hoveredIndex) {
        markAreaDirty(visualRect(index));
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = index;
    }
}

void ListView::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    if (m_hoveredIndex.isValid()) {
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = QModelIndex();
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

    int pixels = 40 * event->delta() / 120;
    m_scrollBar->setValue(m_scrollBar->value() - pixels);
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
    option.state |= QStyle::State_Selected;

    updateTextShadows(palette().color(QPalette::HighlightedText));

    QPainter p(&pixmap);
    foreach (const QModelIndex &index, indexes)
    {
        option.rect = visualRect(index).translated(-boundingRect.topLeft());
        if (index == m_hoveredIndex)
            option.state |= QStyle::State_MouseOver;
        else
            option.state &= ~QStyle::State_MouseOver;
        m_delegate->paint(&p, option, index);
    }
    p.end();

    // Mark the area containing the about-to-be-dragged items as dirty, so they
    // will be erased from the view on the next repaint.  We have to do this
    // before calling QDrag::exec(), since it's a blocking call.
    markAreaDirty(boundingRect);

    // Unset the hovered index so dropEvent won't think the icons are being
    // dropped on a dragged folder.
    m_hoveredIndex = QModelIndex();
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
    option.widget              = 0;
    option.viewItemPosition    = QStyleOptionViewItemV4::OnlyOne;

    if (m_wordWrap) {
        option.features = QStyleOptionViewItemV2::WrapText;
    }

    return option;
}

#include "listview.moc"
