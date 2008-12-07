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

#include "abstractitemview.h"
#include "proxymodel.h"

#include <QItemSelectionModel>
#include <QPaintEngine>
#include <KDirModel>
#include <KFileItemDelegate>

#ifdef Q_WS_X11
#  include <QX11Info>
#  include <X11/Xlib.h>
#endif

AbstractItemView::AbstractItemView(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
      m_lastScrollValue(0),
      m_viewScrolled(false)
{
    m_scrollBar = new Plasma::ScrollBar(Qt::Vertical, this);
    connect(m_scrollBar->nativeWidget(), SIGNAL(valueChanged(int)), SLOT(scrollBarValueChanged(int)));

    int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_iconSize = QSize(size, size);
}

AbstractItemView::~AbstractItemView()
{
}

void AbstractItemView::setModel(QAbstractItemModel *model)
{
    m_model = static_cast<ProxyModel*>(model);
    m_dirModel = static_cast<KDirModel*>(m_model->sourceModel());

    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(modelReset()), SLOT(modelReset()));
    connect(m_model, SIGNAL(layoutChanged()), SLOT(layoutChanged()));
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(dataChanged(QModelIndex,QModelIndex)));
}

QAbstractItemModel *AbstractItemView::model() const
{
    return m_model;
}

void AbstractItemView::setSelectionModel(QItemSelectionModel *model)
{
    m_selectionModel = model;
}

QItemSelectionModel *AbstractItemView::selectionModel() const
{
    return m_selectionModel;
}

void AbstractItemView::setItemDelegate(KFileItemDelegate *delegate)
{
    m_delegate = static_cast<KFileItemDelegate*>(delegate);

    connect(m_delegate, SIGNAL(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)),
            SLOT(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)));
    connect(m_delegate, SIGNAL(commitData(QWidget*)), SLOT(commitData(QWidget*)));
}

KFileItemDelegate *AbstractItemView::itemDelegate() const
{
    return m_delegate;
}

void AbstractItemView::setIconSize(const QSize &iconSize)
{
    m_iconSize = iconSize;
}

QSize AbstractItemView::iconSize() const
{
    return m_iconSize;
}

QScrollBar *AbstractItemView::verticalScrollBar() const
{
    return m_scrollBar->nativeWidget();
}

QRect AbstractItemView::visibleArea() const
{
    return mapToViewport(contentsRect()).toAlignedRect();
}

// Marks the given rect in viewport coordinates, as dirty and schedules a repaint.
void AbstractItemView::markAreaDirty(const QRect &rect)
{
    if (!rect.isEmpty() && rect.intersects(visibleArea())) {
        m_dirtyRegion += rect;
        update(mapFromViewport(rect));
    }
}

// This function scrolls the contents of the backbuffer the distance the scrollbar
// has moved since the last time this function was called.
QRect AbstractItemView::scrollBackBuffer()
{
    int value = m_scrollBar->value();
    int delta = m_lastScrollValue - value;
    m_lastScrollValue = value;

    if (qAbs(delta) >= m_pixmap.height()) {
        return visibleArea();
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

void AbstractItemView::prepareBackBuffer()
{
    const QRect cr = contentsRect().toRect();

    // Make sure the backbuffer pixmap has the same size as the content rect
    if (m_pixmap.size() != cr.size()) {
        QPixmap pixmap(cr.size());
        pixmap.fill(Qt::transparent);
        if (!m_pixmap.isNull()) {
            // Static content optimization
#ifdef Q_WS_X11
            if (m_pixmap.paintEngine()->type() == QPaintEngine::X11) {
                GC gc = XCreateGC(QX11Info::display(), pixmap.handle(), 0, NULL);
                XCopyArea(QX11Info::display(), m_pixmap.handle(), pixmap.handle(), gc, 0, 0,
                          m_pixmap.width(), m_pixmap.height(), 0, 0);
                XFreeGC(QX11Info::display(), gc);
            } else
#endif
            {
                QPainter p(&pixmap);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.drawPixmap(0, 0, m_pixmap);
            }
            QRegion region(pixmap.rect());
            region -= m_pixmap.rect();
            region.translate(0, m_scrollBar->value());
            m_dirtyRegion |= region;
        } else {
            m_dirtyRegion = QRegion(visibleArea());
        }
        m_pixmap = pixmap;
    }

    if (m_viewScrolled) {
        m_dirtyRegion += scrollBackBuffer();
        m_viewScrolled = false;
    }
}

// This function draws the backbuffer pixmap on the widget, and fades out the top
// and bottom if as needed.
void AbstractItemView::syncBackBuffer(QPainter *painter, const QRect &clipRect)
{
    const QRect cr = contentsRect().toRect();

    const int fadeHeight = 16;
    const QRect topFadeRect(cr.x(), cr.y(), cr.width(), fadeHeight);
    const QRect bottomFadeRect(cr.bottomLeft() - QPoint(0, fadeHeight), QSize(cr.width(), fadeHeight));
    int scrollValue = m_scrollBar->value();
    int maxValue = m_scrollBar->maximum();

    // Draw the backbuffer on the widget
    // =================================
    if ((scrollValue > 0 && topFadeRect.intersects(clipRect)) ||
        (scrollValue < maxValue && bottomFadeRect.intersects(clipRect)))
    {
        QPixmap pixmap = m_pixmap;
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        // Fade out the top section of the pixmap if the scrollbar slider isn't at the top
        if (scrollValue > 0 && topFadeRect.intersects(clipRect))
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
        if (scrollValue < maxValue && bottomFadeRect.intersects(clipRect))
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

void AbstractItemView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void AbstractItemView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void AbstractItemView::modelReset()
{
}

void AbstractItemView::layoutChanged()
{
}

void AbstractItemView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
}

void AbstractItemView::commitData(QWidget *editor)
{
    Q_UNUSED(editor)
}

void AbstractItemView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    Q_UNUSED(editor)
    Q_UNUSED(hint)
}


void AbstractItemView::scrollBarValueChanged(int value)
{
    Q_UNUSED(value)

    m_viewScrolled = true;
    update();
}

#include "abstractitemview.moc"
